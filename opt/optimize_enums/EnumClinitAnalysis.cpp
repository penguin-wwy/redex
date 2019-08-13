/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "EnumClinitAnalysis.h"

#include "ConstantPropagationAnalysis.h"
#include "DexUtil.h"
#include "Resolver.h"

/* clang-format off */
/*
 * This module analyzes the clinit of an Enum class in order to determine the
 * ordinal and name values of each Enum instance. The pattern we are targeting
 * is:
 *
 * FooEnum.<clinit>()V:
 *   const/4 v1, #int 0 // ordinal value
 *   const-string v2, "ENUM_NAME"
 *   const-string v3, "SomeOtherData"
 *   new-instance v0, "LFooEnum;"
 *   invoke-direct {v0, v1, v2, v3}, LFooEnum;.<init>:(Ljava/lang/String;ILjava/lang/String)V
 *   sput-object v0, LFooEnum;.ENUM_NAME
 *   ...
 *
 * FooEnum.<init>(Ljava/lang/String;I;Ljava/lang/String)V:
 *   invoke-direct {v0, v1, v2}, Ljava/lang/Enum;.<init>:(Ljava/lang/String;I)V // set the enum name and ordinal
 *   ...
 *
 * The call to Enum.<init> sets the enum's name and ordinal. It's implemented
 * in the Java runtime, so we can't analyze its bytecode, but it can be modeled
 * as setting two private fields in the Enum object.
 */
/* clang-format on */

namespace cp = constant_propagation;

namespace {

DexMethod* get_enum_ctor() {
  return static_cast<DexMethod*>(
      DexMethod::get_method("Ljava/lang/Enum;.<init>:(Ljava/lang/String;I)V"));
}

/*
 * This field does not actually exist -- we are just defining it so we have a
 * way of representing the ordinal value during abstract interpretation.
 */
DexField* get_fake_field(const std::string& full_descriptor) {
  DexField* field =
      static_cast<DexField*>(DexField::make_field(full_descriptor));
  if (field->is_concrete()) {
    field->make_concrete(ACC_PUBLIC);
  }
  return field;
}

DexField* get_ordinal_field() {
  return get_fake_field("Ljava/lang/Enum;.__ordinal__:I");
}

DexField* get_enum_name_field() {
  return get_fake_field("Ljava/lang/Enum;.__name__:Ljava/lang/String;");
}

struct EnumOrdinalAnalyzerState {
  /* implicit */ EnumOrdinalAnalyzerState(const DexType* clinit_class)
      : clinit_class(clinit_class) {
    auto& fields = type_class(clinit_class)->get_ifields();
    enum_instance_fields =
        std::unordered_set<DexField*>(fields.begin(), fields.end());
  }

  const DexMethod* enum_ordinal_init{get_enum_ctor()};

  const DexField* enum_ordinal_field{get_ordinal_field()};

  const DexField* enum_name_field{get_enum_name_field()};

  const DexType* enum_type{get_enum_type()};

  // The Enum class whose <clinit> we are currently analyzing.
  const DexType* clinit_class;

  // The instance fields of the enum whose <clinit> we are analyzing.
  std::unordered_set<DexField*> enum_instance_fields;
};

class EnumOrdinalAnalyzer;

using CombinedAnalyzer = InstructionAnalyzerCombiner<EnumOrdinalAnalyzer,
                                                     cp::HeapEscapeAnalyzer,
                                                     cp::StringAnalyzer,
                                                     cp::PrimitiveAnalyzer>;

class EnumOrdinalAnalyzer
    : public InstructionAnalyzerBase<EnumOrdinalAnalyzer,
                                     ConstantEnvironment,
                                     EnumOrdinalAnalyzerState> {
 public:
  static bool analyze_new_instance(const EnumOrdinalAnalyzerState& state,
                                   const IRInstruction* insn,
                                   ConstantEnvironment* env) {
    auto cls = type_class(insn->get_type());
    if (cls == nullptr) {
      return false;
    }
    if (!is_enum(cls)) {
      return false;
    }
    env->new_heap_value(RESULT_REGISTER, insn, ConstantObjectDomain());
    return true;
  }

  static bool analyze_iput(const EnumOrdinalAnalyzerState& state,
                           const IRInstruction* insn,
                           ConstantEnvironment* env) {
    auto field = resolve_field(insn->get_field());
    if (field == nullptr) {
      return false;
    }
    if (state.enum_instance_fields.count(field)) {
      env->set_object_field(insn->src(1), field, env->get(insn->src(0)));
      return true;
    }
    return false;
  }

  static bool analyze_sput(const EnumOrdinalAnalyzerState& state,
                           const IRInstruction* insn,
                           ConstantEnvironment* env) {
    auto field = resolve_field(insn->get_field());
    if (field == nullptr) {
      return false;
    }
    if (field->get_class() == state.clinit_class) {
      env->set(field, env->get(insn->src(0)));
      return true;
    }
    return false;
  }

  static bool analyze_aput(const EnumOrdinalAnalyzerState& state,
                           const IRInstruction* insn,
                           ConstantEnvironment* env) {
    if (insn->opcode() == OPCODE_APUT_OBJECT) {
      // Simply not do further analysis for the aput-object instructions. Maybe
      // we can improve the analysis in the future.
      return true;
    }
    return false;
  }

  static bool analyze_invoke(const EnumOrdinalAnalyzerState& state,
                             const IRInstruction* insn,
                             ConstantEnvironment* env) {
    auto method = resolve_method(insn->get_method(), opcode_to_search(insn));
    if (method == nullptr) {
      return false;
    }
    if (method == state.enum_ordinal_init) {
      auto name = env->get(insn->src(1));
      auto ordinal = env->get(insn->src(2));
      env->set_object_field(insn->src(0), state.enum_name_field, name);
      env->set_object_field(insn->src(0), state.enum_ordinal_field, ordinal);
      return true;
    } else if (is_init(method) && method->get_class() == state.clinit_class) {
      // TODO(fengliu) : Analyze enums with an instance string field.
      cp::semantically_inline_method(
          method->get_code(),
          insn,
          CombinedAnalyzer(state, nullptr, nullptr, nullptr),
          env);
      return true;
    }
    return false;
  }
};

/**
 * Ordinals should be consecutive and all the static final fields in the enum
 * type are in the result map.
 */
bool validate_result(
    const DexClass* cls,
    std::unordered_map<const DexField*, optimize_enums::EnumAttr>*
        enum_field_to_attrs) {
  if (enum_field_to_attrs->empty()) {
    TRACE(ENUM, 2, "\tEmpty result for %s", SHOW(cls));
    return false;
  }
  std::vector<bool> ordinals(enum_field_to_attrs->size(), false);
  bool synth_values_field = false;

  auto enum_field_access = optimize_enums::enum_field_access();
  auto values_access = optimize_enums::synth_access();

  for (auto field : cls->get_sfields()) {
    auto access = field->get_access();
    auto it = enum_field_to_attrs->find(field);
    if (it != enum_field_to_attrs->end()) {
      if (!check_required_access_flags(enum_field_access, access)) {
        TRACE(ENUM, 2, "\tUnexpected access %x on %s", access, SHOW(field));
        return false;
      }
      auto ordinal = it->second.ordinal;
      if (ordinal > ordinals.size()) {
        TRACE(ENUM, 2, "\tUnexpected ordinal %u on %s", SHOW(ordinal),
              SHOW(field));
        return false;
      }
      ordinals[ordinal] = true;
    } else {
      if (check_required_access_flags(enum_field_access, access)) {
        TRACE(ENUM, 2, "\tEnum value %s is missing in the result",
              SHOW(field));
        return false;
      }
      if (check_required_access_flags(values_access, access)) {
        if (!synth_values_field) {
          synth_values_field = true;
        } else {
          TRACE(ENUM, 2, "\tMultiple static synthetic fields on %s",
                SHOW(cls));
          return false;
        }
      }
    }
  }
  if (std::all_of(ordinals.begin(), ordinals.end(),
                  [](bool item) { return item; })) {
    return true;
  }
  TRACE(ENUM, 2, "\tEnum %s has some values in the same ordinal", SHOW(cls));
  return false;
}

} // namespace

namespace optimize_enums {

DexAccessFlags enum_field_access() { return ACC_STATIC | ACC_ENUM; }

DexAccessFlags synth_access() { return ACC_STATIC | ACC_FINAL | ACC_SYNTHETIC; }

AttrMap analyze_enum_clinit(const DexClass* cls, EnumFieldMap* ifield_map) {
  always_assert(is_enum(cls));

  auto* code = cls->get_clinit()->get_code();
  code->build_cfg(/* editable */ false);
  auto& cfg = code->cfg();
  auto fp_iter = std::make_unique<cp::intraprocedural::FixpointIterator>(
      cfg, CombinedAnalyzer(cls->get_type(), nullptr, nullptr, nullptr));
  fp_iter->run(ConstantEnvironment());

  // XXX we can't use collect_return_state below because it doesn't capture the
  // field environment. We should consider doing away with the field environment
  // and using the heap to model static field values as well, which would
  // simplify code like this.
  auto return_env = ConstantEnvironment::bottom();
  for (cfg::Block* b : cfg.blocks()) {
    auto env = fp_iter->get_entry_state_at(b);
    for (auto& mie : InstructionIterable(b)) {
      auto* insn = mie.insn;
      fp_iter->analyze_instruction(insn, &env);
      if (is_return(insn->opcode())) {
        return_env.join_with(env);
      }
    }
  }

  AttrMap enum_field_to_attrs;
  if (!return_env.get_field_environment().is_value()) {
    return enum_field_to_attrs;
  }
  auto ordinal_field = get_ordinal_field();
  auto enum_name_field = get_enum_name_field();
  for (auto& pair : return_env.get_field_environment().bindings()) {
    auto* field = pair.first;
    if (field->get_class() != cls->get_type()) {
      continue;
    }
    auto heap_ptr = pair.second.maybe_get<AbstractHeapPointer>();
    if (!heap_ptr) {
      continue;
    }
    auto ptr = return_env.get_pointee<ConstantObjectDomain>(*heap_ptr);
    auto ordinal = ptr.get<SignedConstantDomain>(ordinal_field);
    auto ordinal_value = ordinal.get_constant();
    if (!ordinal_value) {
      continue;
    }
    always_assert(*ordinal_value >= 0);

    auto name = ptr.get<StringDomain>(enum_name_field);
    auto name_value = name.get_constant();
    if (!name_value) {
      continue;
    }

    EnumAttr enum_obj{static_cast<uint32_t>(*ordinal_value), *name_value};
    enum_field_to_attrs.emplace(field, enum_obj);

    for (auto* enum_ifield : cls->get_ifields()) {
      EnumFieldValue& ifield_value = (*ifield_map)[enum_ifield][*ordinal_value];
      auto env_value = ptr.get(enum_ifield);
      if (enum_ifield->get_type() == get_string_type()) {
        if (auto string_ptr = env_value.maybe_get<SignedConstantDomain>()) {
          if (auto string_ptr_value = string_ptr->get_constant()) {
            always_assert(*string_ptr_value == 0);
            // The `Ljava/lang/String` value is a `null` constant.
            ifield_value.string_value = nullptr;
            continue;
          }
        } else if (auto string_value = env_value.maybe_get<StringDomain>()) {
          if (auto string_const = string_value->get_constant()) {
            ifield_value.string_value = *string_const;
            continue;
          }
        }
      } else { // `enum_ifield` is a primitive type
        if (auto primitive_value =
                env_value.maybe_get<SignedConstantDomain>()) {
          if (auto primitive_const = primitive_value->get_constant()) {
            ifield_value.primitive_value = *primitive_const;
            continue;
          }
        }
      }

      TRACE(ENUM, 9,
            "Reject enum %s because we could not find constant value of "
            "instance field %s",
            SHOW(cls), SHOW(enum_ifield));
      enum_field_to_attrs.clear();
      ifield_map->clear();
      return enum_field_to_attrs;
    }
  }
  if (!validate_result(cls, &enum_field_to_attrs)) {
    enum_field_to_attrs.clear();
    ifield_map->clear();
  }
  return enum_field_to_attrs;
}

} // namespace optimize_enums
