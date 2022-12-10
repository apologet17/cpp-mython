#include "statement.h"

#include <iostream>
#include <exception>
#include <sstream>

using namespace std;

namespace ast {

using runtime::Closure;
using runtime::Context;
using runtime::ObjectHolder;

namespace {
const string ADD_METHOD = "__add__"s;
const string INIT_METHOD = "__init__"s;
}  // namespace


// Присваивает переменной, имя которой задано в параметре var, значение выражения rv
ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
  closure[var_] = rv_.get()->Execute(closure, context);

  return closure[var_];
}

Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv)
  : var_ (var),
    rv_ (std::move(rv)) {
}

VariableValue::VariableValue(const std::string& var_name) : var_name_(var_name) {
}

VariableValue::VariableValue(std::vector<std::string> dotted_ids) : dotted_ids_(std::move(dotted_ids)) {
}

ObjectHolder VariableValue::Execute(Closure& closure, Context& context) {
  if (!dotted_ids_.empty()) {
    if(dotted_ids_.size() == 1)
      return VariableValue(dotted_ids_.front()).Execute(closure, context);


    auto first = dotted_ids_.front();

    auto object = closure[first];
    Closure& new_closure = object.TryAs<runtime::ClassInstance>()->Fields();


    std::vector<std::string> new_dotted_ids;

    for (int i = 1; i < static_cast<int>(dotted_ids_.size()); ++i) {
      new_dotted_ids.push_back(dotted_ids_[i]);
    }

    return VariableValue(new_dotted_ids).Execute(new_closure, context);
  }




  if (closure.count(var_name_)) {
    return closure[var_name_];
  }


  throw std::runtime_error("VariableValue fail"s);
}

unique_ptr<Print> Print::Variable(const std::string& name) {
  Print print;
  print.SetName(name);
  return make_unique<Print>(std::move(print));
}

Print::Print(unique_ptr<Statement> argument) : value_(std::move(argument)) {
}

Print::Print(vector<unique_ptr<Statement>> args) : value_(std::move(args)) {
}

ObjectHolder Print::Execute(Closure& closure, Context& context) {
  if (!name_.empty()) {
    closure[name_].Get()->Print(context.GetOutputStream(), context);
  } else if (holds_alternative<std::unique_ptr<Statement>>(value_)) {
    std::get<std::unique_ptr<Statement>>(value_).get()->Execute(closure, context).Get()->Print(context.GetOutputStream(), context);
  } else if (holds_alternative<std::vector<std::unique_ptr<Statement>>>(value_)) {

    bool is_first_element = true;
    auto& values = std::get<std::vector<std::unique_ptr<Statement>>>(value_);

    if (values.size() == 1) {
      auto object = values.front().get()->Execute(closure, context);

              if (!object) {
                context.GetOutputStream() << "None"sv;
              } else {
                object.Get()->Print(context.GetOutputStream(), context);
              }

    } else {

      for (const auto& stmnt : values) {
        if (!is_first_element) {
          context.GetOutputStream() << " "sv;
        }

        auto object = stmnt.get()->Execute(closure, context);

        if (!object) {
          context.GetOutputStream() << "None"sv;
          is_first_element = false;
          continue;
        }

        object.Get()->Print(context.GetOutputStream(), context);

        is_first_element = false;
      }
    }


  }

  context.GetOutputStream() << endl;

  return {};
}

MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
                       std::vector<std::unique_ptr<Statement>> args)
: object_(std::move(object)),
method_(std::move(method)) ,
args_(std::move(args)) {
}

ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {

  std::vector<runtime::ObjectHolder> args;

  for (const auto& arg : args_) {
    args.push_back(arg.get()->Execute(closure, context));
  }


  return object_.get()->Execute(closure, context).TryAs<runtime::ClassInstance>()->Call(method_, args, context);
}

ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
  string result;

  auto arg = GetArgument().get()->Execute(closure, context);

  if (!arg) {
    result = "None"s;
  }

  if (arg.TryAs<runtime::Number>() != nullptr) {
    result = std::to_string(arg.TryAs<runtime::Number>()->GetValue());
  }

  if (arg.TryAs<runtime::Bool>() != nullptr) {
      result = arg.TryAs<runtime::Bool>()->GetValue() ? "True"s : "False"s;
  }

  if (arg.TryAs<runtime::String>() != nullptr) {
    result = arg.TryAs<runtime::String>()->GetValue();
  }

  if (arg.TryAs<runtime::ClassInstance>() != nullptr) {

    if (arg.TryAs<runtime::ClassInstance>()->HasMethod("__str__"s, 0)) {
      auto object = arg.TryAs<runtime::ClassInstance>()->Call("__str__"s, {}, context);

      if (object.TryAs<runtime::Number>() != nullptr) {
        result = std::to_string(object.TryAs<runtime::Number>()->GetValue());
      }

      if (object.TryAs<runtime::Bool>() != nullptr) {

        if (object.TryAs<runtime::Bool>()->GetValue()) {
          result = "True"s;
        } else {
          result = "False"s;
        }

      }

      if (object.TryAs<runtime::String>() != nullptr) {
        result = object.TryAs<runtime::String>()->GetValue();
      }

    } else {
      std::ostringstream oss;
      oss  << arg.Get();
      result = oss.str();
    }
  }



  return ObjectHolder::Own(runtime::String(result));
}

ObjectHolder Add::Execute(Closure& closure, Context& context) {
  auto lhs = GetLhs().get()->Execute(closure, context);
  auto rhs = GetRhs().get()->Execute(closure, context);

  if (lhs.TryAs<runtime::Number>() != nullptr && rhs.TryAs<runtime::Number>() != nullptr) {
    auto result = lhs.TryAs<runtime::Number>()->GetValue() + rhs.TryAs<runtime::Number>()->GetValue();
    return runtime::ObjectHolder().Own(runtime::Number(result));
  }

  if (lhs.TryAs<runtime::String>() != nullptr && rhs.TryAs<runtime::String>() != nullptr) {
    auto result = lhs.TryAs<runtime::String>()->GetValue() + rhs.TryAs<runtime::String>()->GetValue();
    return runtime::ObjectHolder().Own(runtime::String(result));
  }

  if (lhs.TryAs<runtime::ClassInstance>() != nullptr) {
    if (lhs.TryAs<runtime::ClassInstance>()->HasMethod(ADD_METHOD, 1)) {
      return lhs.TryAs<runtime::ClassInstance>()->Call(ADD_METHOD, {rhs}, context);;
    }
  }

  throw runtime_error("Add method fail"s);
}

ObjectHolder Sub::Execute(Closure& closure, Context& context) {
  auto lhs = GetLhs().get()->Execute(closure, context);
  auto rhs = GetRhs().get()->Execute(closure, context);

  if (lhs.TryAs<runtime::Number>() != nullptr && rhs.TryAs<runtime::Number>() != nullptr) {
    auto result = lhs.TryAs<runtime::Number>()->GetValue() - rhs.TryAs<runtime::Number>()->GetValue();
    return runtime::ObjectHolder().Own(runtime::Number(result));
  }

  throw runtime_error("Sub method fail"s);

}

ObjectHolder Mult::Execute(Closure& closure, Context& context) {
  auto lhs = GetLhs().get()->Execute(closure, context);
  auto rhs = GetRhs().get()->Execute(closure, context);

  if (lhs.TryAs<runtime::Number>() != nullptr && rhs.TryAs<runtime::Number>() != nullptr) {
    auto result = lhs.TryAs<runtime::Number>()->GetValue() * rhs.TryAs<runtime::Number>()->GetValue();
    return runtime::ObjectHolder().Own(runtime::Number(result));
  }

  throw runtime_error("Mult method fail"s);
}

ObjectHolder Div::Execute(Closure& closure, Context& context) {
  auto lhs = GetLhs().get()->Execute(closure, context);
  auto rhs = GetRhs().get()->Execute(closure, context);

  if (lhs.TryAs<runtime::Number>() != nullptr && rhs.TryAs<runtime::Number>() != nullptr) {

    if (rhs.TryAs<runtime::Number>()->GetValue() != 0) {
      auto result = lhs.TryAs<runtime::Number>()->GetValue() / rhs.TryAs<runtime::Number>()->GetValue();
      return runtime::ObjectHolder().Own(runtime::Number(result));
    }
  }

  throw runtime_error("Div method fail"s);
}

ObjectHolder Compound::Execute(Closure& closure, Context& context) {

  if (!statements_.empty()) {

    for (const auto& stmt : statements_) {
      stmt.get()->Execute(closure, context);
    }
  }


  return {};
}

ObjectHolder Return::Execute(Closure& closure, Context& context) {
  auto object = statement_.get()->Execute(closure, context);
  std::stringstream result;
  object.Get()->Print(result, context);
  throw runtime_error(result.str());
    // Заглушка. Реализуйте метод самостоятельно
    return {};
}

ClassDefinition::ClassDefinition(ObjectHolder cls) : cls_(std::move(cls)) {
}

// Создаёт внутри closure новый объект, совпадающий с именем класса и значением, переданным в
// конструктор

ObjectHolder ClassDefinition::Execute(Closure& closure, [[maybe_unused]] Context& context) {
    closure[cls_.TryAs<runtime::Class>()->GetName()] = cls_;
    return closure[cls_.TryAs<runtime::Class>()->GetName()];
}

FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
                                 std::unique_ptr<Statement> rv)
  : object_(std::move(object)),
    field_name_(std::move(field_name)),
    rv_(std::move(rv)) {
}

ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
  auto object = object_.Execute(closure, context);
  Closure& new_closure = object.TryAs<runtime::ClassInstance>()->Fields();


  auto rv = rv_.get()->Execute(closure, context);
    new_closure[field_name_] = rv_.get()->Execute(closure, context);

  return new_closure[field_name_];
}

IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
               std::unique_ptr<Statement> else_body) :   condition_(std::move(condition)),
if_body_(std::move(if_body)),
else_body_(std::move(else_body)) {
}

ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
  if (condition_.get()->Execute(closure, context).TryAs<runtime::Bool>()->GetValue()) {
    return if_body_.get()->Execute(closure, context);
  } else if (else_body_.get() != nullptr) {
    return else_body_.get()->Execute(closure, context);
  }
    return {};
}

ObjectHolder Or::Execute(Closure& closure, Context& context) {
  auto lhs = GetLhs().get()->Execute(closure, context).TryAs<runtime::Bool>();

  if (lhs != nullptr) {
    if (lhs->GetValue())
      return runtime::ObjectHolder().Own(runtime::Bool(true));
  }

  auto rhs = GetRhs().get()->Execute(closure, context).TryAs<runtime::Bool>();

  if (rhs != nullptr) {
      return runtime::ObjectHolder().Own(runtime::Bool(rhs->GetValue()));
  }

  throw runtime_error("Or method fail"s);
}

ObjectHolder And::Execute(Closure& closure, Context& context) {
  auto lhs = GetLhs().get()->Execute(closure, context).TryAs<runtime::Bool>();

  if (lhs != nullptr) {
    if (lhs->GetValue() == false)
      return runtime::ObjectHolder().Own(runtime::Bool(false));
  }

  auto rhs = GetRhs().get()->Execute(closure, context).TryAs<runtime::Bool>();

  if (rhs != nullptr) {
      return runtime::ObjectHolder().Own(runtime::Bool(lhs->GetValue() && rhs->GetValue()));
  }

  throw runtime_error("Add method fail"s);
}

ObjectHolder Not::Execute(Closure& closure, Context& context) {
  auto argument = GetArgument().get()->Execute(closure, context).TryAs<runtime::Bool>();

  if (argument != nullptr) {
      return runtime::ObjectHolder().Own(runtime::Bool(!argument->GetValue()));
  }

  throw runtime_error("Not method fail"s);
}

Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
    : BinaryOperation(std::move(lhs), std::move(rhs)),
      cmp_(std::move(cmp)) {
}

ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
  auto lhs = GetLhs().get()->Execute(closure, context);
  auto rhs = GetRhs().get()->Execute(closure, context);

  return runtime::ObjectHolder().Own(runtime::Bool(cmp_(lhs, rhs, context)));
}

NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args) : class__(class_),
    args_(std::move(args)){
}

NewInstance::NewInstance(const runtime::Class& class_) : class__(class_) {
}

ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
  auto result = runtime::ObjectHolder::Own(runtime::ClassInstance(class__));

  if (result.TryAs<runtime::ClassInstance>()->HasMethod(INIT_METHOD, args_.size())) {
    std::vector<runtime::ObjectHolder> convert_arg;

    for (const auto& arg : args_) {
      convert_arg.push_back(arg.get()->Execute(closure, context));
    }

    result.TryAs<runtime::ClassInstance>()->Call(INIT_METHOD, convert_arg, context);
  }

    return result;
}

MethodBody::MethodBody(std::unique_ptr<Statement>&& body) : body_(std::move(body))  {
}

ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
  try {
    body_.get()->Execute(closure, context);
  } catch (runtime_error& e) {
    return ObjectHolder::Own(runtime::String(e.what()));
  }




  return {};
}

}  // namespace ast