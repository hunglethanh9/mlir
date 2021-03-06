// RUN: mlir-tblgen -gen-op-defs -I %S/../../include %s | FileCheck %s --check-prefix=DEF
// RUN: mlir-tblgen -gen-rewriters -I %S/../../include %s | FileCheck %s --check-prefix=PAT

include "mlir/IR/OpBase.td"

def NS_SomeEnum_A : EnumAttrCase<"A">;
def NS_SomeEnum_B : EnumAttrCase<"B">;
def NS_SomeEnum_C : EnumAttrCase<"C">;

def NS_SomeEnum : EnumAttr<
  "SomeEnum", "some enum",
  [NS_SomeEnum_A, NS_SomeEnum_B, NS_SomeEnum_C]>;

def NS_OpA : Op<"op_a_with_enum_attr", []> {
  let arguments = (ins NS_SomeEnum:$attr);
}

// DEF-LABEL: StringRef OpA::attr()
// DEF-NEXT:    auto attr = this->getAttr("attr").dyn_cast_or_null<StringAttr>();
// DEF-NEXT:    return attr.getValue();

// DEF-LABEL: OpA::verify()
// DEF:       if (!(((this->getAttr("attr").cast<StringAttr>().getValue() == "A")) || ((this->getAttr("attr").cast<StringAttr>().getValue() == "B")) || ((this->getAttr("attr").cast<StringAttr>().getValue() == "C"))))
// DEF-SAME:    return emitOpError("attribute 'attr' failed to satisfy some enum attribute constraints");

def NS_OpB : Op<"op_b_with_enum_attr", []> {
  let arguments = (ins NS_SomeEnum:$attr);
}

def : Pat<(NS_OpA NS_SomeEnum_A:$attr), (NS_OpB NS_SomeEnum_B)>;

// PAT-LABEL: struct GeneratedConvert0
// PAT:       PatternMatchResult match
// PAT:         if (!((op0->getAttrOfType<StringAttr>("attr").cast<StringAttr>().getValue() == "A"))) return matchFailure();
// PAT:       void rewrite
// PAT:         auto vOpB0 = rewriter.create<NS::OpB>(loc,
// PAT-NEXT:        rewriter.getStringAttr("B")
// PAT-NEXT:    );
