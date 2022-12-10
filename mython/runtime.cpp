#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>
#include <variant>

using namespace std;

namespace runtime {

ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
    : data_(std::move(data)) {
}

void ObjectHolder::AssertIsValid() const {
    assert(data_ != nullptr);
}

ObjectHolder ObjectHolder::Share(Object& object) {
    // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
    return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
}

ObjectHolder ObjectHolder::None() {
    return ObjectHolder();
}

Object& ObjectHolder::operator*() const {
    AssertIsValid();
    return *Get();
}

Object* ObjectHolder::operator->() const {
    AssertIsValid();
    return Get();
}

Object* ObjectHolder::Get() const {
    return data_.get();
}

ObjectHolder::operator bool() const {
    return Get() != nullptr;
}

bool IsTrue(const ObjectHolder& object) {

    if (auto ptr = object.TryAs<Number>(); (ptr != nullptr) && (ptr->GetValue() != 0)) {
        return true;
    }
   
    if (auto ptrs = object.TryAs<String>(); (ptrs != nullptr) && (!ptrs->GetValue().empty())) {
        return true;
    }
    
    if (auto ptrb = object.TryAs<Bool>(); (ptrb != nullptr) && (ptrb->GetValue())) {
        return true;
    }

    return false;
}

void ClassInstance::Print(std::ostream& os, Context& context) {
    if (this->HasMethod("__str__"s, 0)) {
       this->Call("__str__"s, {}, context).Get()->Print(os, context);
    }
    else {
        os << this;
    }
}

bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
    auto method_ptr = class_.GetMethod(method);
    if (method_ptr != nullptr) {
        return method_ptr->formal_params.size() == argument_count;
    }

    return false;
}

Closure& ClassInstance::Fields() {
    return closure_;
}

const Closure& ClassInstance::Fields() const {
    return closure_;
}

ClassInstance::ClassInstance(const Class& cls) 
        :class_(cls){
}

ObjectHolder ClassInstance::Call(const std::string& method,
                                 const std::vector<ObjectHolder>& actual_args,
                                 Context& context) {
    auto method_ptr = class_.GetMethod(method);

    if (method_ptr != nullptr && method_ptr->formal_params.size() == actual_args.size()) {

        Closure temp;
        temp["self"] = ObjectHolder::Share(*this);

        for (int i = 0; i < static_cast<int>(actual_args.size()); ++i) {
            temp[method_ptr->formal_params[i]] = actual_args[i];
        }

        return method_ptr->body.get()->Execute(temp, context);;
    }
     throw std::runtime_error("Strange Method"s); ;
}

Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
        : name_(name)
        , parent_(parent){
    for ( auto& method : methods) {
        methods_[method.name] = std::move(method);
    }

    if (parent == nullptr) {
        return;
    }

    for (auto& method : parent->methods_) {
        if (methods_.count(method.first) && 
           (methods_.at(method.first).formal_params.size() == method.second.formal_params.size())) {
            continue;
        }
        methods_ptr_[method.first] = &(parent->methods_.at(method.first));
    }
}

const Method* Class::GetMethod(const std::string& name) const {
    if (methods_.count(name))  {
        return &(methods_.at(name));
    }
    else if (methods_ptr_.count(name)) {
        return methods_ptr_.at(name);
    }

    return nullptr;
}

[[nodiscard]] const std::string& Class::GetName() const {
    return name_;
}

void Class::Print(ostream& os, [[maybe_unused]] Context& context) {
    os <<"Class "s<< GetName();
}

void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
    os << (GetValue() ? "True"sv : "False"sv);
}

bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (lhs.Get() == nullptr && rhs.Get() == nullptr)
        return true;

    if (lhs.TryAs<String>() != nullptr && rhs.TryAs<String>() != nullptr) {
        return lhs.TryAs<String>()->GetValue() == rhs.TryAs<String>()->GetValue();
    }

    if (lhs.TryAs<Number>() != nullptr && rhs.TryAs<Number>() != nullptr) {
        return lhs.TryAs<Number>()->GetValue() == rhs.TryAs<Number>()->GetValue();
    }

    if (lhs.TryAs<Bool>() != nullptr && rhs.TryAs<Bool>() != nullptr) {
        return lhs.TryAs<Bool>()->GetValue() == rhs.TryAs<Bool>()->GetValue();
    }

    if (lhs.TryAs<ClassInstance>() != nullptr) {
        auto ptr = lhs.TryAs<ClassInstance>();
        if (ptr->HasMethod("__eq__"s, 1)) {
            ObjectHolder result = ptr->Call("__eq__", { rhs }, context);

            if (result.TryAs<Bool>() != nullptr) {
                return result.TryAs<Bool>()->GetValue();
            }
        }

    }

    throw std::runtime_error("Cannot compare objects for equality"s);
}

bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (lhs.TryAs<String>() != nullptr && rhs.TryAs<String>() != nullptr) {
        return lhs.TryAs<String>()->GetValue() < rhs.TryAs<String>()->GetValue();
    }

    if (lhs.TryAs<Number>() != nullptr && rhs.TryAs<Number>() != nullptr) {
        return lhs.TryAs<Number>()->GetValue() < rhs.TryAs<Number>()->GetValue();
    }

    if (lhs.TryAs<Bool>() != nullptr && rhs.TryAs<Bool>() != nullptr) {
        return lhs.TryAs<Bool>()->GetValue() < rhs.TryAs<Bool>()->GetValue();
    }

    if (lhs.TryAs<ClassInstance>() != nullptr) {
        auto ptr = lhs.TryAs<ClassInstance>();
        if (ptr->HasMethod("__lt__"s, 1)) {
            ObjectHolder result = ptr->Call("__lt__", { rhs }, context);

            if (result.TryAs<Bool>() != nullptr) {
                return result.TryAs<Bool>()->GetValue();
            }
        }

    }

    throw std::runtime_error("Cannot compare objects for less"s);
}

bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {  
    return !Equal(lhs, rhs, context);
}

bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context) && NotEqual(lhs, rhs, context);
}

bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Greater(lhs, rhs, context);
}

bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context);
}

}  // namespace runtime