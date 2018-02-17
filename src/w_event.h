#ifndef WGUI_W_EVENT_H
#define WGUI_W_EVENT_H

#include <functional>
#include <vector>
#include <tuple>
#include <unordered_set>
#include <map>
#include <utility>
#include <type_traits>
#include <mutex>
#include <thread>
#include <tuple>
#include <experimental/tuple>

namespace wevents
    {
    template<class... Args>
    class WSignal;

    class WSlotObject;

    namespace internal
        {
        namespace events
            {
            class MutexActions;

            class ThreadActions;
            }
        }

    class ConOps
        {
    private:
        internal::events::MutexActions *mutexActions;
        internal::events::ThreadActions *threadActions;

        void release_resources();

    public:
        ConOps();

        ~ConOps()
            { release_resources(); }

        ConOps(const ConOps &copy);

        ConOps(ConOps &&copy)
                : mutexActions(copy.mutexActions),
                  threadActions(copy.threadActions)
            {
            copy.mutexActions = nullptr;
            copy.threadActions = nullptr;
            }

        ConOps &operator=(const ConOps &copy);

        ConOps &operator=(ConOps &&copy)
            {
            release_resources();
            mutexActions = copy.mutexActions;
            threadActions = copy.threadActions;
            copy.mutexActions = nullptr;
            copy.threadActions = nullptr;
            return *this;
            }

        ConOps &blocking(bool value);

        ConOps &mutex(std::mutex &mutex);

        bool is_blocking() const
            { return threadActions == nullptr; }

        bool has_mutex() const
            { return mutexActions == nullptr; }

        internal::events::ThreadActions &get_thread_actions()
            { return *threadActions; }

        internal::events::MutexActions &get_mutex()
            { return *mutexActions; }
        };

    namespace internal
        {
        namespace events
            {
            class ConnectionBase
                {
            public:
                enum States
                    {
                    DEFAULT,
                    RUNNING,
                    DELETE_SELF,
                    };

            private:
                States state;
                ConOps options;

            protected:
                ConnectionBase(ConOps &&options)
                        : options(options),
                          state(ConnectionBase::DEFAULT)
                    {}

            public:
                virtual ~ConnectionBase()
                    {}

                ConOps &get_options()
                    { return options; }

                void set_state(States state)
                    { this->state = state; }

                States get_state() const
                    { return state; }
                };

            template<class... Args>
            class Connection;

            inline void register_connection(WSlotObject *object, ConnectionBase *ptr);
            inline void unregister_connection(WSlotObject *object, ConnectionBase *ptr);

            class MutexActions
                {
            public:
                virtual void execute(std::function<void()> code) = 0;
                virtual MutexActions *clone() = 0;
                };

            class Mutex : public MutexActions
                {
            private:
                std::mutex *mutex;

            public:
                Mutex(std::mutex &mutex)
                        : mutex(&mutex)
                    {}

                void execute(std::function<void()> code)
                    {
                    mutex->lock();
                    code();
                    mutex->unlock();
                    }

                MutexActions *clone()
                    { return new Mutex(*mutex); }
                };

            class NoMutex : public MutexActions
                {
            public:
                void execute(std::function<void()> code)
                    {}

                MutexActions *clone()
                    { return new NoMutex(); }
                };

            class ThreadActions
                {
            public:
                virtual void execute(std::function<void()> code, ConnectionBase *conn) = 0;
                virtual ThreadActions *clone() = 0;
                };

            class Thread : public ThreadActions
                {
            public:
                Thread()
                    {}

                void execute(std::function<void()> code, ConnectionBase *conn)
                    {
                    conn->set_state(ConnectionBase::RUNNING);
                    std::thread thread(
                            [&, conn]()
                                {
                                code();
                                switch (conn->get_state())
                                    {
                                    case ConnectionBase::RUNNING:
                                        conn->set_state(ConnectionBase::DEFAULT);
                                        break;
                                    case ConnectionBase::DELETE_SELF:
                                        delete conn;
                                        break;
                                    default:
                                        break;
                                    }
                                }
                    );
                    thread.detach();
                    }

                ThreadActions *clone()
                    { return new Thread(); }
                };

            class NoThread : public ThreadActions
                {
            public:
                NoThread()
                    {}

                void execute(std::function<void()> code, ConnectionBase *conn)
                    { code(); }

                ThreadActions *clone()
                    { return new NoThread(); }
                };
            }
        }

    class WSlotObject
        {
    private:
        friend inline void internal::events::register_connection(WSlotObject *, ConnectionBase *);
        friend inline void internal::events::unregister_connection(WSlotObject *, ConnectionBase *);

        std::unordered_set<internal::events::ConnectionBase *> connections;

    protected:
        WSlotObject()
            {}

    public:
        virtual ~WSlotObject()
            {
            const size_t ARRAY_SIZE = connections.size();
            internal::events::ConnectionBase **conn_copy = new internal::events::ConnectionBase *[ARRAY_SIZE];

            {
                size_t i = 0;
                for (internal::events::ConnectionBase *connection : connections)
                    {
                    conn_copy[i] = connection;
                    i++;
                    }
            }

            for (size_t i = 0; i < ARRAY_SIZE; i++)
                {
                if (conn_copy[i]->get_options().is_blocking()
                    && (conn_copy[i]->get_state() == internal::events::ConnectionBase::RUNNING))
                    { conn_copy[i]->set_state(internal::events::ConnectionBase::DELETE_SELF); }
                else
                    { delete conn_copy[i]; }
                }

            delete[] conn_copy;
            }
        };

    ConOps::ConOps(const ConOps &copy)
            : mutexActions(copy.mutexActions->clone()),
              threadActions(copy.threadActions->clone())
        {}

    ConOps &ConOps::operator=(const ConOps &copy)
        {
        release_resources();
        mutexActions = copy.mutexActions->clone();
        threadActions = copy.threadActions->clone();
        return *this;
        }

    ConOps::ConOps()
            : mutexActions(new internal::events::NoMutex()),
              threadActions(new internal::events::NoThread())
        {}

    ConOps &ConOps::blocking(bool value)
        {
        if (value)
            { threadActions = new internal::events::NoThread(); }
        else
            { threadActions = new internal::events::Thread(); }
        return *this;
        }

    ConOps &ConOps::mutex(std::mutex &mutex)
        {
        delete mutexActions;
        mutexActions = new typename internal::events::Mutex(mutex);
        return *this;
        }

    void ConOps::release_resources()
        {
        if (mutexActions != nullptr)
            { delete mutexActions; }
        if (threadActions != nullptr)
            { delete threadActions; }
        }

    namespace internal
        {
        namespace events
            {
            inline void register_connection(WSlotObject *object, ConnectionBase *ptr)
                { object->connections.insert(ptr); }

            inline void unregister_connection(WSlotObject *object, ConnectionBase *ptr)
                { object->connections.erase(ptr); }

            template<class... Args>
            class Connection : public ConnectionBase
                {
            private:
                WSignal<Args...> *signal;

            protected:
                Connection(WSignal<Args...> *signal, ConOps &&options)
                        : ConnectionBase(std::move(options)),
                          signal(signal)
                    { signal->register_connection((Connection<Args...> *) this); }

                virtual void call_impl(std::tuple<Args...> &args) = 0;

            public:
                virtual ~Connection()
                    { signal->unregister_connection((Connection<Args...> *) this); }

                void call(std::tuple<Args...> *args)
                    {
                    //auto my_invokable = std::bind(call_impl, this, args);
                    //my_invokable.operator()();
                    get_options().get_thread_actions().execute(
                            [this, args]()
                                {
                                get_options().get_mutex().execute(
                                        [this, args]()
                                            { this->call_impl(*args); }
                                );
                                }, this
                    );
                    }
                };

            template<class... Args>
            class SignalCallbackConnection : public Connection<Args...>
                {
            private:
                std::function<void(Args...)> callback;

            protected:
                void call_impl(std::tuple<Args...> &args)
                    { std::experimental::apply(callback, args); }

            public:
                SignalCallbackConnection(
                        WSignal<Args...> *signal,
                        ConOps &&options,
                        std::function<void(Args...)> callback
                                        )
                        : Connection<Args...>(signal, std::move(options)),
                          callback(callback)
                    {}
                };

            template<class T, class... Args>
            class MethodFunctor
                {
            private:
                T *object;
                void (T::*callback)(Args...);

            public:
                MethodFunctor(T *object, void(T::*callback)(Args...))
                        : object(object),
                          callback(callback)
                    {}

                template<class... ArgTypes>
                void operator()(ArgTypes &&... args) const
                    { (object->*callback)(std::forward<ArgTypes>(args)...); }
                };

            template<class T, class Enable, class... Args>
            class SignalObjectMethodConnection_impl;

            template<class T, class... Args>
            class SignalObjectMethodConnection_impl
                    <T, typename std::enable_if<std::is_base_of<WSlotObject, T>::value>::type, Args...>
                    : public Connection<Args...>
                {
            private:
                T *object;
                void (T::*callback)(Args...);

            protected:
                void call_impl(std::tuple<Args...> &args)
                    {
                    MethodFunctor<T, Args...> invokable(object, callback);
                    std::experimental::apply(invokable, args);
                    }

            public:
                SignalObjectMethodConnection_impl(
                        WSignal<Args...> *signal,
                        ConOps &&options,
                        void (T::*callback)(Args...),
                        T *object
                                                 )
                        : Connection<Args...>(signal, std::move(options)),
                          callback(callback),
                          object(object)
                    { register_connection(static_cast<WSlotObject *>(object), static_cast<ConnectionBase *>(this)); }

                virtual ~SignalObjectMethodConnection_impl()
                    { unregister_connection(static_cast<WSlotObject *>(object), static_cast<ConnectionBase *>(this)); }
                };

            template<class T, class... Args>
            class SignalObjectMethodConnection : public SignalObjectMethodConnection_impl<T, void, Args...>
                {
            public:
                SignalObjectMethodConnection(
                        WSignal<Args...> *signal,
                        ConOps &&options,
                        void (T::*callback)(Args...),
                        T *object
                                            )
                        : SignalObjectMethodConnection_impl<T, void, Args...>(
                        signal,
                        std::move(options),
                        callback,
                        object
                )
                    {}
                };

            template<class T, class Enable, class... Args>
            class SignalObjectLifetimeConnection_impl;

            template<class T, class... Args>
            class SignalObjectLifetimeConnection_impl
                    <T, typename std::enable_if<std::is_base_of<WSlotObject, T>::value>::type, Args...>
                    : public Connection<Args...>
                {
            private:
                T *object;
                std::function<void(Args...)> callback;

            protected:
                void call_impl(std::tuple<Args...> &args)
                    { std::experimental::apply(callback, args); }

            public:
                SignalObjectLifetimeConnection_impl(
                        WSignal<Args...> *signal,
                        ConOps &&options,
                        std::function<void(Args...)> callback,
                        T *object
                                                   )
                        : Connection<Args...>(signal, std::move(options)),
                          callback(callback),
                          object(object)
                    { register_connection(static_cast<WSlotObject *>(object), static_cast<ConnectionBase *>(this)); }

                virtual ~SignalObjectLifetimeConnection_impl()
                    { unregister_connection(static_cast<WSlotObject *>(object), static_cast<ConnectionBase *>(this)); }
                };

            template<class T, class... Args>
            class SignalObjectLifetimeConnection : public SignalObjectLifetimeConnection_impl<T, void, Args...>
                {
            public:
                SignalObjectLifetimeConnection(
                        WSignal<Args...> *signal,
                        ConOps &&options,
                        std::function<void(Args...)> callback,
                        T *object
                                              )
                        : SignalObjectLifetimeConnection_impl<T, void, Args...>(
                        signal,
                        std::move(options),
                        callback,
                        object
                )
                    {}
                };
            }
        }

    namespace internal
        {
        namespace events
            {
            template<class T>
            struct Identity
                {
                typedef T type;
                };
            }
        }

    template<class... Args>
    std::function<void()> connect(
            WSignal<Args...> &signal,
            typename internal::events::Identity<std::function<void(Args...)> >::type callback,
            ConOps options = {}
                                 )
        {
        internal::events::Connection<Args...> *connection = new internal::events::SignalCallbackConnection<Args...>(
                &signal,
                std::move(
                        options
                ),
                callback
        );
        return [&]()
            { delete connection; };
        }

    template<class T, class... Args>
    std::function<void()> connect(
            WSignal<Args...> &signal,
            typename internal::events::Identity<void (T::*)(Args...)>::type callback,
            T *ptr,
            ConOps options = {}
                                 )
        {
        internal::events::Connection<Args...> *connection = new internal::events::SignalObjectMethodConnection<T,
                                                                                                               Args...>(
                &signal,
                std::move(options),
                callback,
                ptr
        );
        return [&]()
            { delete connection; };
        }

    template<class T, class... Args>
    std::function<void()> connect(
            WSignal<Args...> &signal,
            std::function<void(Args...)> callback,
            T *ptr,
            ConOps options = {}
                                 )
        {
        internal::events::Connection<Args...> *connection = new internal::events::SignalObjectLifetimeConnection<T,
                                                                                                                 Args...>(
                &signal,
                std::move(options),
                callback,
                ptr
        );
        return [&]()
            { delete connection; };
        }

    namespace internal
        {
        namespace events
            {
            template<class... Types>
            struct pack
                {
                };

            template<class T1, class T2>
            struct valid_arg_types;

            template<class Arg1, class... Args1, class Arg2, class... Args2>
            struct valid_arg_types<pack<Arg1, Args1...>, pack<Arg2, Args2...> >
                    : std::conditional_t<std::is_convertible<Arg1, Arg2>::value,
                                         valid_arg_types<pack<Args1...>, pack<Args2...> >,
                                         std::false_type>::type
                {
                };

            template<>
            struct valid_arg_types<pack<>, pack<> >
                    : std::true_type
                {
                };
            }
        }

    template<class... Args>
    class WSignal : public WSlotObject
        {
    private:

        friend class internal::events::Connection<Args...>;

        std::unordered_set<internal::events::Connection<Args...> *> connections;

        void register_connection(internal::events::Connection<Args...> *ptr)
            { connections.insert(ptr); }

        void unregister_connection(internal::events::Connection<Args...> *ptr)
            { connections.erase(ptr); }

    public:
        ~WSignal()
            {
            const size_t ARRAY_SIZE = connections.size();
            internal::events::ConnectionBase **conn_copy = new internal::events::ConnectionBase *[ARRAY_SIZE];

            {
                size_t i = 0;
                for (internal::events::ConnectionBase *connection : connections)
                    {
                    conn_copy[i] = connection;
                    i++;
                    }
            }

            for (size_t i = 0; i < ARRAY_SIZE; i++)
                {
                if (conn_copy[i]->get_options().is_blocking()
                    && (conn_copy[i]->get_state() == internal::events::ConnectionBase::RUNNING))
                    { conn_copy[i]->set_state(internal::events::ConnectionBase::DELETE_SELF); }
                else
                    { delete conn_copy[i]; }
                }

            delete[] conn_copy;
            }

        template<class... ArgTypes>
        void emit(ArgTypes &&... args)
            {
            static_assert(
                    internal::events::valid_arg_types<internal::events::pack<ArgTypes &&...>,
                                                      internal::events::pack<Args...> >::value,
                    "one of your arguments in not the correct type"
            );

            std::vector<internal::events::Connection<Args...> *> conn_copy;
            for (internal::events::Connection<Args...> *connection : connections)
                { conn_copy.push_back(connection); }

            std::tuple<Args...> tup(args...);
            for (internal::events::Connection<Args...> *connection : conn_copy)
                { connection->call(&tup); }
            }
        };

    template<class... Args>
    std::function<void()> connect(WSignal<Args...> &signal1, WSignal<Args...> &signal2, ConOps options = {})
        {
        internal::events::Connection<Args...> *connection = new internal::events::SignalObjectMethodConnection<WSignal<
                Args...>, Args...>(&signal1, std::move(options), &WSignal<Args...>::emit, &signal2);
        return [&]()
            { delete connection; };
        }
    }

#endif //WGUI_W_EVENT_H

