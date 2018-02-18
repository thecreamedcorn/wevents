//
// Created by Administrator on 9/23/2017.
//

#ifndef WGUI_W_PROPERTY_H
#define WGUI_W_PROPERTY_H

#include <vector>
#include <memory>
#include <type_traits>
#include <functional>
#include <tuple>

#include "w_event(old).h"

namespace wevents
    {
    template<class T>
    class WProperty;

    namespace internal
        {
        namespace property
            {
            template<class T>
            class ValueBase
                {
            protected:
                ValueBase()
                    {}

            public:
                virtual ~ValueBase()
                    {}

                virtual const T &get_immutable() const = 0;
                virtual T &get_mutable() = 0;
                };

            template<class T>
            class MutableValue : public ValueBase<T>
                {
            private:
                T value;

            public:
                MutableValue(const T &copy)
                        : value(copy)
                    {}

                template<class... Args>
                MutableValue(Args &&... args)
                        : value(std::forward<Args>(args)...)
                    {}

                const T &get_immutable() const
                    { return value; }

                T &get_mutable()
                    { return value; }
                };

            template<class T>
            class MutableValuePointer : public ValueBase<T>
                {
            private:
                std::unique_ptr<T> value;

            public:
                MutableValuePointer(T *&&value)
                        : value(std::move(value))
                    {}

                const T &get_immutable() const
                    { return value; }

                T &get_mutable()
                    { return *(value.get()); }
                };

            template<class T>
            class ImmutableValue : public ValueBase<T>
                {
            private:
                ValueBase<T> **value_ref;

            protected:
                ImmutableValue(ValueBase<T> **value_ref)
                        : value_ref(value_ref)
                    {}

            public:
                virtual ~ImmutableValue()
                    {}

                T &get_mutable()
                    {
                    ValueBase<T> *temp = new MutableValue<T>(this->get_immutable());
                    *value_ref = temp;
                    delete this;
                    return temp->get_mutable();
                    }
                };

            template<class T>
            class PropertyBinding : public ImmutableValue<T>, public WSlotObject
                {
            private:
                WProperty<T> *binding;

                void SLOT_property_deleted(const WProperty<T> &value)
                    { this->get_mutable(); }

            public:
                PropertyBinding(ValueBase<T> **value_ref, WProperty<T> *binding, WProperty<T> *parent)
                        : ImmutableValue<T>(value_ref),
                          binding(binding)
                    {
                    connect(binding->onDeleted, &PropertyBinding<T>::SLOT_property_deleted, this);
                    connect(
                            binding->onChanged, [&](const T &value)
                                { parent->onChanged.emit(this->get_immutable()); }, this
                    );
                    }

                PropertyBinding(
                        ValueBase<T> **value_ref,
                        WProperty<T> *binding,
                        std::function<void(const T &)> callback
                               )
                        : ImmutableValue<T>(value_ref),
                          binding(binding)
                    {
                    connect(binding->onDeleted, &PropertyBinding<T>::SLOT_property_deleted, this);
                    connect(binding->onChanged, callback, this);
                    }

                const T &get_immutable() const
                    { return binding->get(); }
                };

            template<class Signature>
            class ExprBinding;

            template<std::size_t N, typename... Ts> using NthTypeOf = typename std::tuple_element<N,
                                                                                                  std::tuple<Ts...>>::type;

            template<std::size_t count>
            struct connect_all
                {
                template<class T, class... Args>
                static inline void run(
                        ExprBinding<T(Args...)> *value,
                        std::tuple<ValueBase<Args> *...> &result,
                        std::tuple<WProperty<Args> *...> &args
                                      )
                    {
                    std::get<count - 1>(result) = new PropertyBinding<NthTypeOf<count - 1, Args...> >(
                            &(std::get<count - 1>(result)),
                            std::get<count - 1>(args),
                            ([=](auto arg)
                                { value->value_update(); })
                    );
                    connect_all<count - 1>::run(value, result, args);
                    }
                };

            template<>
            struct connect_all<0>
                {
                template<class T, class... Args>
                static inline void run(
                        ExprBinding<T(Args...)> *value,
                        std::tuple<ValueBase<Args> *...> &result,
                        std::tuple<WProperty<Args> *...> &args
                                      )
                    {}
                };

            template<std::size_t size>
            struct call
                {
                template<class T, class... Args, class... Collection>
                static T run(
                        std::function<T(Args...)> expr,
                        std::tuple<ValueBase<Args> *...> &args,
                        Collection &&... collection
                            )
                    {
                    return call<size - 1>::run(
                            expr,
                            args,
                            std::get<size - 1>(args)->get_immutable(),
                            std::forward<Collection>(collection)...
                    );
                    }
                };

            template<>
            struct call<0>
                {
                template<class T, class... Args, class... Collection>
                static T run(
                        std::function<T(Args...)> expr,
                        std::tuple<ValueBase<Args> *...> &args,
                        Collection &&... collection
                            )
                    { return expr(std::forward<Collection>(collection)...); }
                };

            template<std::size_t count>
            struct delete_tuple
                {
                template<class... Args>
                static inline void run(std::tuple<Args *...> &tuple)
                    {
                    delete std::get<count - 1>(tuple);
                    delete_tuple<count - 1>::run(tuple);
                    }
                };

            template<>
            struct delete_tuple<0>
                {
                template<class... Args>
                static inline void run(std::tuple<Args *...> &tuple)
                    {}
                };

            template<class T, class... Args>
            class ExprBinding<T(Args...)> : public ImmutableValue<T>, public WSlotObject
                {
            private:
                typedef std::function<T(Args...)> func_type;

                std::tuple<ValueBase<Args> *...> bindings;
                std::unique_ptr<T> value;
                WProperty<T> *parent;
                func_type expr;

                static const std::size_t argNum = sizeof...(Args);

            public:
                ExprBinding(ValueBase<T> **value_ref, WProperty<T> *parent, func_type expr, WProperty<Args> &... args)
                        : ImmutableValue<T>(value_ref),
                          expr(expr),
                          parent(parent)
                    {
                    auto args_tuple = std::make_tuple<WProperty<Args> *...>((&args)...);
                    connect_all<argNum>::run/*<T, Args...>*/(this, bindings, args_tuple);
                    value = std::make_unique<T>(call<sizeof...(Args)>::run(expr, bindings));
                    }

                void value_update()
                    {
                    value = std::make_unique<T>(call<sizeof...(Args)>::run(expr, bindings));
                    parent->onChanged.emit(get_immutable());
                    }

                const T &get_immutable() const
                    { return *value; }
                };
            }
        }

    template<class T>
    class WProperty
        {
    private:
        internal::property::ValueBase<T> *value;

    public:
        WProperty(const T &copy)
                : value(new internal::property::MutableValue<T>(copy))
            {}

        WProperty(T &&move)
                : value(new internal::property::MutableValue<T>(std::move(move)))
            {}

        WProperty(T *&&moveptr)
                : value(new internal::property::MutableValuePointer<T>(std::move(moveptr)))
            {}

        WProperty(WProperty<T> &binding)
            { this->value = new internal::property::PropertyBinding<T>(&value, &binding, this); }

        template<class... Args>
        WProperty(
                typename internal::events::Identity<std::function<T(Args...)> >::type callback,
                WProperty<Args> &... args
                 )
            { this->value = new internal::property::ExprBinding<T(Args...)>(&value, this, callback, args...); }

        ~WProperty()
            {
            onDeleted.emit(*this);
            delete value;
            }

        WSignal<const T &> onChanged;
        WSignal<const WProperty<T> &> onDeleted;

        void operate(std::function<void(typename std::add_lvalue_reference<T>::type)> func)
            {
            func(value->get_mutable());
            onChanged.emit(value->get_immutable());
            }

        WProperty<T> &operator=(const T &eq)
            {
            value = new internal::property::MutableValue<T>(eq);
            onChanged.emit(value->get_immutable());
            return *this;
            }

        WProperty<T> &operator=(T &&eq)
            {
            value = new internal::property::MutableValue<T>(std::move(eq));
            onChanged.emit(value->get_immutable());
            return *this;
            }

        WProperty<T> &operator=(T *&&eq)
            {
            value = new internal::property::MutableValuePointer<T>(std::move(eq));
            onChanged.emit(value->get_immutable());
            return *this;
            }

        WProperty<T> &operator=(WProperty<T> &binding)
            {
            this->value = new internal::property::PropertyBinding<T>(&value, &binding, this);
            onChanged.emit(value->get_immutable());
            return *this;
            }

        template<class... Args>
        WProperty<T> &set_expr(
                typename internal::events::Identity<std::function<T(Args...)> >::type callback,
                WProperty<Args> &... args
                              )
            {
            this->value = new internal::property::ExprBinding<T(Args...)>(&value, this, callback, args...);
            onChanged.emit(value->get_immutable());
            return *this;
            }

        const T &get() const
            { return value->get_immutable(); }
        };
    }

#endif //WGUI_W_PROPERTY_H

