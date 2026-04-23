#include "CodeGenerator.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

CodeGenerator::CodeGenerator(std::ostream& out) : out(out) {}

void CodeGenerator::setSourceInfo(const std::string& filename, const std::vector<std::string>& lines) {
    sourceFilename = filename;
    sourceLines = lines;
}

void CodeGenerator::embedSource(ASTNode& node) {
    if (node.line <= 0 || node.line > (int)sourceLines.size()) return;
    if (node.line == lastEmbeddedLine) return;
    lastEmbeddedLine = node.line;
    out << "; [" << sourceFilename << ":" << node.line << "] " << sourceLines[node.line - 1] << std::endl;
}

void CodeGenerator::generate(TranslationUnit& unit) {
    emitter = std::make_unique<M65Emitter>(out, zeroPageStart);
    zpRegs.clear();
    for (uint32_t i = 0; i < zeroPageAvail; ++i) zpRegs.push_back({false});
    invalidateRegs();
    resultNeeded = true;
    unit.accept(*this);
    emitData();
}

void CodeGenerator::emit(const std::string& line) {
    out << "    " << line << std::endl;
}

std::string CodeGenerator::newLabel() {
    return "L" + std::to_string(labelCount++);
}

std::string CodeGenerator::newDontCareLabel() {
    return "@L" + std::to_string(labelCount++);
}

bool CodeGenerator::isStruct(const std::string& type) {
    if (type.length() >= 7 && type.substr(0, 7) == "struct ") return true;
    if (type.length() >= 6 && type.substr(0, 6) == "union ") return true;
    return false;
}

std::string CodeGenerator::getAggregateName(const std::string& type) {
    if (type.length() >= 7 && type.substr(0, 7) == "struct ") return type.substr(7);
    if (type.length() >= 6 && type.substr(0, 6) == "union ") return type.substr(6);
    return type;
}

std::string CodeGenerator::resolveVarName(const std::string& name) {
    if (name.length() >= 3) {
        std::string prefix = name.substr(0, 3);
        if (prefix == "_p_" || prefix == "_l_" || prefix == "_g_") {
            return name;
        }
    }
    std::string pName = "_p_" + name;
    if (variableTypes.count(pName)) return pName;
    std::string lName = "_l_" + name;
    if (variableTypes.count(lName)) return lName;
    std::string gName = "_g_" + name;
    if (globalVariableTypes.count(gName)) return gName;
    return name;
}

CodeGenerator::ExpressionType CodeGenerator::getExprType(Expression* expr) {
    if (auto* ref = dynamic_cast<VariableReference*>(expr)) {
        std::string rName = resolveVarName(ref->name);
        if (variableTypes.count(rName)) {
            VarInfo& vi = variableTypes.at(rName);
            if (vi.arraySize >= 0) return {vi.type, vi.pointerLevel + 1};
            return {vi.type, vi.pointerLevel};
        }
        if (globalVariableTypes.count("_g_" + ref->name)) {
            VarInfo& vi = globalVariableTypes.at("_g_" + ref->name);
            if (vi.arraySize >= 0) return {vi.type, vi.pointerLevel + 1};
            return {vi.type, vi.pointerLevel};
        }
    }
    if (auto* aa = dynamic_cast<ArrayAccess*>(expr)) {
        ExpressionType base = getExprType(aa->arrayExpr.get());
        if (base.pointerLevel > 0) return {base.type, base.pointerLevel - 1};
        return {base.type, 0};
    }
    if (auto* cond = dynamic_cast<ConditionalExpression*>(expr)) {
        return getExprType(cond->thenExpr.get());
    }
    if (auto* bin = dynamic_cast<BinaryOperation*>(expr)) { return getExprType(bin->left.get()); }
    if (auto* un = dynamic_cast<UnaryOperation*>(expr)) {
        if (un->op == "!") return {"char", 0};
        CodeGenerator::ExpressionType sub = getExprType(un->operand.get());
        if (un->op == "*") return {sub.type, sub.pointerLevel > 0 ? sub.pointerLevel - 1 : 0};
        if (un->op == "&") return {sub.type, sub.pointerLevel + 1};
        return sub;
    }
    if (auto* ma = dynamic_cast<MemberAccess*>(expr)) {
        ExpressionType baseType = getExprType(ma->structExpr.get());
        if (!isStruct(baseType.type)) {
             if (auto* ref = dynamic_cast<VariableReference*>(ma->structExpr.get())) {
                 if (globalVariableTypes.count("_g_" + ref->name)) {
                     baseType = {globalVariableTypes.at("_g_" + ref->name).type, globalVariableTypes.at("_g_" + ref->name).pointerLevel};
                 }
             }
        }
        if (isStruct(baseType.type)) {
            std::string sName = getAggregateName(baseType.type);
            if (structs.count(sName)) {
                auto& sInfo = *structs[sName];
                if (sInfo.members.count(ma->memberName)) {
                    MemberInfo& mInfo = sInfo.members[ma->memberName];
                    if (mInfo.arraySize >= 0) return {mInfo.type, mInfo.pointerLevel + 1};
                    return {mInfo.type, mInfo.pointerLevel};
                }
            }
        }
    }
    return {"int", 0};
}

void CodeGenerator::emitAddress(Expression* expr) {
    if (auto* ref = dynamic_cast<VariableReference*>(expr)) {
        emit("ptrstack " + resolveVarName(ref->name));
    } else if (auto* ma = dynamic_cast<MemberAccess*>(expr)) {
        if (ma->isArrow) {
            bool oldNeeded = resultNeeded;
            resultNeeded = true;
            ma->structExpr->accept(*this);
            resultNeeded = oldNeeded;
        } else {
            emitAddress(ma->structExpr.get());
        }
        ExpressionType baseType = getExprType(ma->structExpr.get());
        if (!isStruct(baseType.type)) {
             if (auto* ref = dynamic_cast<VariableReference*>(ma->structExpr.get())) {
                 if (globalVariableTypes.count("_g_" + ref->name)) {
                     baseType = {globalVariableTypes.at("_g_" + ref->name).type, globalVariableTypes.at("_g_" + ref->name).pointerLevel};
                 }
             }
        }
        if (isStruct(baseType.type)) {
            std::string sName = getAggregateName(baseType.type);
            if (structs.count(sName)) {
                auto& sInfo = *structs[sName];
                if (sInfo.members.count(ma->memberName)) {
                    MemberInfo& mInfo = sInfo.members[ma->memberName];
                    if (mInfo.offset > 0) {
                        emitter->add_16_imm(mInfo.offset);
                    }
                }
            }
        }
    } else if (auto* aa = dynamic_cast<ArrayAccess*>(expr)) {
        aa->arrayExpr->accept(*this); // Get base address in AX
        int zpBase = allocateZP(2);
        std::stringstream ss;
        ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpBase);
        emit("stax $" + ss.str());
        
        bool oldNeeded = resultNeeded;
        resultNeeded = true;
        aa->indexExpr->accept(*this); // Get index in AX
        resultNeeded = oldNeeded;
        
        ExpressionType baseType = getExprType(aa->arrayExpr.get());
        int elementSize = 1;
        if (baseType.pointerLevel > 1 || baseType.type == "int") {
            elementSize = 2;
        } else if (isStruct(baseType.type)) {
             std::string sName = getAggregateName(baseType.type);
             if (structs.count(sName)) elementSize = structs[sName]->totalSize;
        }

        if (elementSize == 2) {
            emitter->asl_a(); emitter->pha(); emitter->txa(); emitter->rol_a(); emitter->tax(); emitter->pla();
        } else if (elementSize > 2) {
             int zpIdx = allocateZP(2);
             std::stringstream ssM; ssM << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpIdx);
             emit("stax $" + ssM.str());
             std::stringstream ssVal; ssVal << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << elementSize;
             emit("ldax #$" + ssVal.str());
             emit("mul.16 .ax, $" + ssM.str());
             freeZP(zpIdx, 2);
        }

        std::stringstream ss2;
        ss2 << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpBase);
        emit("add.16 .ax, $" + ss2.str());
        freeZP(zpBase, 2);
    } else if (auto* un = dynamic_cast<UnaryOperation*>(expr)) {
        if (un->op == "*") {
            bool oldNeeded = resultNeeded;
            resultNeeded = true;
            un->operand->accept(*this);
            resultNeeded = oldNeeded;
        }
    }
    invalidateRegs();
}

void CodeGenerator::visit(TranslationUnit& node) {
    out << "; Generated by cc45" << std::endl;
    out << ".segmentOrder code, data, bss" << std::endl;
    out << ".code" << std::endl;
    out << ".org $2000" << std::endl << std::endl;
    for (auto& decl : node.topLevelDecls) decl->accept(*this);
}

void CodeGenerator::visit(FunctionDeclaration& node) {
    out << ".code" << std::endl;
    out << "proc " << node.name << std::endl;
    variableTypes.clear();
    currentVars.clear();
    std::string procLine = "    .proc " + node.name;

    currentFunction = &node;

    for (auto& param : node.parameters) {
        std::string pName = "_p_" + param.name;
        variableTypes[pName] = {param.type, param.pointerLevel, param.isVolatile};
        if (param.pointerLevel > 0) {
            procLine += ", W#" + pName;
        } else if (param.type == "char") {
            procLine += ", B#" + pName;
        } else {
            procLine += ", W#" + pName;
        }
        currentVars.push_back(pName);
    }
    out << procLine << std::endl;
    node.body->accept(*this);
    if (!node.isNoreturn) {
        // Check if the last statement in the body was a return statement.
        // If it was, we don't need to emit an implicit one.
        bool lastWasReturn = false;
        if (!node.body->statements.empty()) {
            if (dynamic_cast<ReturnStatement*>(node.body->statements.back().get())) {
                lastWasReturn = true;
            }
        }
        if (!lastWasReturn) {
            emit("rtn #$00");
        }
    }
    emit("endproc");
    out << std::endl;

    currentFunction = nullptr;
}

void CodeGenerator::visit(CompoundStatement& node) {
    for (auto& stmt : node.statements) stmt->accept(*this);
}

void CodeGenerator::visit(VariableDeclaration& node) {
    embedSource(node);

    if (node.isGlobal || currentFunction == nullptr) {
        std::string gName = "_g_" + node.name;
        globalVariableTypes[gName] = {node.type, node.pointerLevel, node.isVolatile, node.arraySize};
        if (node.isGlobal) {
            globalVars.push_back(&node);
            return;
        }
    }

    std::string lName = "_l_" + node.name;
    variableTypes[lName] = {node.type, node.pointerLevel, node.isVolatile, node.arraySize};
    int size = 0;
    if (node.pointerLevel > 0) size = 2;
    else if (node.type == "char") size = 1;
    else if (node.type == "int") size = 2;
    else if (isStruct(node.type)) {
        std::string sName = getAggregateName(node.type);
        if (structs.count(sName)) size = structs[sName]->totalSize;
        else throw std::runtime_error("Unknown struct/union type: " + sName);
    }
    if (node.arraySize >= 0) size *= node.arraySize;

    if (node.isVolatile) {
        if (node.initializer) {
            if (auto* lit = dynamic_cast<IntegerLiteral*>(node.initializer.get())) {
                 if (size == 2) {
                    std::stringstream ss;
                    ss << "#$" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << (uint16_t)(int16_t)lit->value;
                    emit("phw.imm " + ss.str());
                } else {
                    emitter->lda_imm(lit->value & 0xFF);
                    emitter->pha();
                }
            } else {
                bool oldNeeded = resultNeeded;
                resultNeeded = true;
                node.initializer->accept(*this);
                resultNeeded = oldNeeded;
                if (size == 2) emitter->push_ax();
                else emitter->pha();
            }
        } else {
            if (size == 2) emit("phw.imm #$0000");
            else { emitter->lda_imm(0); emitter->pha(); }
        }
    } else {
        
        if (node.initializer && dynamic_cast<IntegerLiteral*>(node.initializer.get())) {
            if (!isVariableUsed(node.name, *currentFunction)) {
                return;
            }
        }
        
        if (node.initializer) {
            if (auto* lit = dynamic_cast<IntegerLiteral*>(node.initializer.get())) {
                if (size == 2) {
                    std::stringstream ss;
                    ss << "#$" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << (uint16_t)(int16_t)lit->value;
                    emit("phw.imm " + ss.str());
                } else {
                    emitter->lda_imm(lit->value & 0xFF);
                    emitter->pha();
                }
            } else {
                bool oldNeeded = resultNeeded;
                resultNeeded = true;
                node.initializer->accept(*this);
                resultNeeded = oldNeeded;
                if (size == 2) emitter->push_ax();
                else emitter->pha();
            }
        } else {
            if (size >= 9) {
                emit("phw.imm #$0000");
                for (int i = 0; i < (size - 2) / 2; ++i) emit("phw.imm #$0000");
                if (size % 2) { emit("lda #0"); emit("pha"); }
                emit("lda #0");
                emit("FILL.SP " + lName + ", #" + std::to_string(size));
            } else {
                if (size == 2) emit("phw.imm #$0000");
                else { emitter->lda_imm(0); emitter->pha(); }
            }
        }
    }
    for (const auto& varName : currentVars) {
        emit(".var " + varName + " = " + varName + " + " + std::to_string(size));
    }
    emit(".var " + lName + " = " + std::to_string(size));
    currentVars.push_back(lName);
    invalidateRegs();
}

void CodeGenerator::visit(Assignment& node) {
    embedSource(node);
    if (auto* ref = dynamic_cast<VariableReference*>(node.target.get())) {
        std::string rName = resolveVarName(ref->name);
        bool isGlobal = (rName.length() >= 3 && rName.substr(0, 3) == "_g_");
        std::string suffix = isGlobal ? "" : ", s";

        if (auto* bin = dynamic_cast<BinaryOperation*>(node.expression.get())) {
            if (bin->op == "+" || bin->op == "-") {
                if (auto* leftRef = dynamic_cast<VariableReference*>(bin->left.get())) {
                    if (auto* rightLit = dynamic_cast<IntegerLiteral*>(bin->right.get())) {
                        if (resolveVarName(leftRef->name) == rName && rightLit->value == 1) {
                            VarInfo vi = variableTypes.count(rName) ? variableTypes.at(rName) : globalVariableTypes.at(rName);
                            bool is16 = (vi.pointerLevel > 0 || vi.type == "int");
                            if (isStruct(vi.type)) {
                                std::string sName = getAggregateName(vi.type);
                                if (structs.count(sName) && structs[sName]->totalSize > 1) is16 = true;
                            }
                            if (bin->op == "+") {
                                if (is16) emit("inw " + rName + suffix);
                                else emit("inc " + rName + suffix);
                            } else {
                                if (is16) emit("dew " + rName + suffix);
                                else emit("dec " + rName + suffix);
                            }
                            invalidateRegs();
                            if (resultNeeded) ref->accept(*this);
                            return;
                        }
                    }
                }
            }
        }

        if (auto* lit = dynamic_cast<IntegerLiteral*>(node.expression.get())) {
            VarInfo vi = variableTypes.count(rName) ? variableTypes.at(rName) : globalVariableTypes.at(rName);
            bool is16 = (vi.pointerLevel > 0 || vi.type == "int");
            if (isStruct(vi.type)) {
                std::string sName = getAggregateName(vi.type);
                if (structs.count(sName) && structs[sName]->totalSize > 1) is16 = true;
            }
            if (is16) {
                std::stringstream ss;
                ss << "#$" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << (uint16_t)(int16_t)lit->value;
                if (isGlobal) {
                    emit("stw " + ss.str() + ", " + rName);
                } else {
                    emit("stw.sp " + ss.str() + ", " + rName);
                }
                updateRegAVar(rName, 0); regA.value = lit->value & 0xFF; regA.isVariable = true;
                updateRegXVar(rName, 1); regX.value = (lit->value >> 8) & 0xFF; regX.isVariable = true;
            } else {
                uint8_t val = lit->value & 0xFF;
                if (!regA.known || (regA.isVariable && regA.varName != rName) || (!regA.isVariable && regA.value != val)) {
                    emitter->lda_imm(val);
                }
                emit("sta " + rName + suffix);
                updateRegAVar(rName, 0); regA.value = val;
            }
            return;
        }

        VarInfo vi = variableTypes.count(rName) ? variableTypes.at(rName) : globalVariableTypes.at(rName);
        if (isStruct(vi.type)) {
            std::string sName = getAggregateName(vi.type);
            if (structs.count(sName)) {
                int structSize = structs[sName]->totalSize;
                if (structSize >= 9) {
                    if (auto* sourceRef = dynamic_cast<VariableReference*>(node.expression.get())) {
                        std::string srcName = resolveVarName(sourceRef->name);
                        bool sourceGlobal = (srcName.length() >= 3 && srcName.substr(0, 3) == "_g_");
                        
                        std::stringstream ssX, ssY;
                        ssX << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (structSize & 0xFF);
                        ssY << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << ((structSize >> 8) & 0xFF);
                        emit("ldx #$" + ssX.str());
                        emit("ldy #$" + ssY.str());
                        emit("MOVE " + srcName + (sourceGlobal ? "" : ", s") + ", " + rName + (isGlobal ? "" : ", s"));
                        invalidateVar(rName);
                        return;
                    }
                }
            }
        }

        bool oldNeeded = resultNeeded;
        resultNeeded = true;
        node.expression->accept(*this);
        resultNeeded = oldNeeded;

        bool is16 = (vi.pointerLevel > 0 || vi.type == "int");
        if (isStruct(vi.type)) {
            std::string sName = getAggregateName(vi.type);
            if (structs.count(sName) && structs[sName]->totalSize > 1) is16 = true;
        }
        if (is16) {
            emit("stax " + rName + suffix);
            updateRegAVar(rName, 0); updateRegXVar(rName, 1);
        } else {
            emit("sta " + rName + suffix);
            updateRegAVar(rName, 0);
        }
        return;
    } else if (auto* ma = dynamic_cast<MemberAccess*>(node.target.get())) {
        if (!ma->isArrow) {
            if (auto* ref = dynamic_cast<VariableReference*>(ma->structExpr.get())) {
                std::string rName = resolveVarName(ref->name);
                bool isGlobal = (rName.length() >= 3 && rName.substr(0, 3) == "_g_");
                std::string suffix = isGlobal ? "" : ", s";

                ExpressionType baseType = getExprType(ref);
                if (!isStruct(baseType.type)) {
                    if (globalVariableTypes.count("_g_" + ref->name)) {
                        baseType = {globalVariableTypes.at("_g_" + ref->name).type, globalVariableTypes.at("_g_" + ref->name).pointerLevel};
                    }
                }
                if (isStruct(baseType.type)) {
                    std::string sName = getAggregateName(baseType.type);
                    if (structs.count(sName) && structs[sName]->members.count(ma->memberName)) {
                        MemberInfo& mInfo = structs[sName]->members[ma->memberName];
                        bool oldNeeded = resultNeeded;
                        resultNeeded = true;
                        node.expression->accept(*this);
                        resultNeeded = oldNeeded;
                        bool is16 = (mInfo.pointerLevel > 0 || mInfo.type == "int");
                        if (isStruct(mInfo.type)) {
                            std::string nestedSName = getAggregateName(mInfo.type);
                            if (structs.count(nestedSName) && structs[nestedSName]->totalSize > 1) is16 = true;
                        }
                        if (is16) {
                            emit("stax " + rName + "+" + std::to_string(mInfo.offset) + suffix);
                            updateRegAVar(rName, mInfo.offset); updateRegXVar(rName, mInfo.offset + 1);
                        } else {
                            emit("sta " + rName + "+" + std::to_string(mInfo.offset) + suffix);
                            updateRegAVar(rName, mInfo.offset);
                        }
                        invalidateRegs();
                        return;
                    }
                }
            }
        }
    }

    emitAddress(node.target.get());
    int zpIdx = allocateZP(2);
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpIdx);
    emit("stax $" + ss.str());
    bool oldNeeded = resultNeeded;
    resultNeeded = true;
    node.expression->accept(*this);
    resultNeeded = oldNeeded;
    emitter->sta_ind_z(emitter->getZP(zpIdx), false);
    updateRegY(0);
    ExpressionType targetType = getExprType(node.target.get());
    bool is16 = (targetType.pointerLevel > 0 || targetType.type == "int");
    if (isStruct(targetType.type)) {
        std::string sName = getAggregateName(targetType.type);
        if (structs.count(sName) && structs[sName]->totalSize > 1) is16 = true;
    }
    if (is16) {
        emitter->txa(); regA.known = false;
        emitter->ldy_imm(1); updateRegY(1);
        std::stringstream ss2;
        ss2 << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpIdx);
        emit("sta ($" + ss2.str() + "),y");
    }
    freeZP(zpIdx, 2);
    invalidateRegs();
}

void CodeGenerator::visit(BinaryOperation& node) {
    ExpressionType lhsType = getExprType(node.left.get());
    ExpressionType rhsType = getExprType(node.right.get());
    int scale = 1;
    if (lhsType.pointerLevel > 0 && rhsType.pointerLevel == 0 && (node.op == "+" || node.op == "-")) {
        scale = (lhsType.type == "char" && lhsType.pointerLevel == 1) ? 1 : 2;
    }
    bool isLiteralOne = false;
    if (scale == 1) {
        if (auto* lit = dynamic_cast<IntegerLiteral*>(node.right.get())) {
            if (lit->value == 1) isLiteralOne = true;
        }
    }
    if (isLiteralOne && (node.op == "+" || node.op == "-")) {
        bool oldNeeded = resultNeeded;
        resultNeeded = true;
        node.left->accept(*this);
        resultNeeded = oldNeeded;
        if (node.op == "+") {
            std::string label = newDontCareLabel();
            emitter->inc_a(); emitter->bne(0x02); emit("inx");
            out << label << ":" << std::endl;
        } else {
            std::string label = newDontCareLabel();
            emit("cmp #$00"); emit("bne " + label); emit("dex");
            out << label << ":" << std::endl; emitter->dec_a();
        }
        invalidateRegs();
        return;
    }
    if (node.op == "||") {
        std::string labelTrue = newDontCareLabel();
        std::string labelEnd = newDontCareLabel();
        bool oldNeeded = resultNeeded;
        resultNeeded = true;
        node.left->accept(*this);
        resultNeeded = oldNeeded;
        if (getExprType(node.left.get()).type == "char" && getExprType(node.left.get()).pointerLevel == 0) {
            if (flags.znSource != FlagSource::A) emit("cmp #$00");
            emit("bne " + labelTrue);
        } else {
            if (flags.znSource != FlagSource::A) emit("cmp #$00");
            emit("branch.16 bne, " + labelTrue);
        }
        resultNeeded = true;
        node.right->accept(*this);
        resultNeeded = oldNeeded;
        if (getExprType(node.right.get()).type == "char" && getExprType(node.right.get()).pointerLevel == 0) {
            if (flags.znSource != FlagSource::A) emit("cmp #$00");
            emit("bne " + labelTrue);
        } else {
            if (flags.znSource != FlagSource::A) emit("cmp #$00");
            emit("branch.16 bne, " + labelTrue);
        }
        emit("zero a, x");
        emit("bra " + labelEnd);
        out << labelTrue << ":" << std::endl;
        emit("lda #$01");
        emit("ldx #$00");
        updateRegA(1); updateRegX(0);
        out << labelEnd << ":" << std::endl;
        invalidateRegs();
        return;
    } else if (node.op == "&&") {
        std::string labelFalse = newDontCareLabel();
        std::string labelEnd = newDontCareLabel();
        bool oldNeeded = resultNeeded;
        resultNeeded = true;
        node.left->accept(*this);
        resultNeeded = oldNeeded;
        if (getExprType(node.left.get()).type == "char" && getExprType(node.left.get()).pointerLevel == 0) {
            if (flags.znSource != FlagSource::A) emit("cmp #$00");
            emit("beq " + labelFalse);
        } else {
            if (flags.znSource != FlagSource::A) emit("cmp #$00");
            emit("branch.16 beq, " + labelFalse);
        }
        resultNeeded = true;
        node.right->accept(*this);
        resultNeeded = oldNeeded;
        if (getExprType(node.right.get()).type == "char" && getExprType(node.right.get()).pointerLevel == 0) {
            if (flags.znSource != FlagSource::A) emit("cmp #$00");
            emit("beq " + labelFalse);
        } else {
            if (flags.znSource != FlagSource::A) emit("cmp #$00");
            emit("branch.16 beq, " + labelFalse);
        }
        emit("lda #$01");
        emit("ldx #$00");
        updateRegA(1); updateRegX(0);
        emit("bra " + labelEnd);
        out << labelFalse << ":" << std::endl;
        emit("zero a, x");
        updateRegA(0); updateRegX(0);
        out << labelEnd << ":" << std::endl;
        invalidateRegs();
        return;
    }

    bool isMultiplicativeLiteral = false;
    if (dynamic_cast<IntegerLiteral*>(node.right.get())) {
        if (node.op == "*" || node.op == "/" || node.op == "%" || node.op == "<<" || node.op == ">>") {
            isMultiplicativeLiteral = true;
        }
    }

    bool oldNeeded = resultNeeded;
    resultNeeded = true;
    node.left->accept(*this);

    if (isMultiplicativeLiteral) {
        int val = dynamic_cast<IntegerLiteral*>(node.right.get())->value;
        if (node.op == "*") {
            if (val == 0) { emit("zero a, x"); updateRegA(0); updateRegX(0); }
            else if (val == 1) { /* Already in AX */ }
            else if (val > 0 && (val & (val - 1)) == 0) {
                int shifts = 0; while (val > 1) { val >>= 1; shifts++; }
                if (shifts == 1 && getExprType(node.left.get()).pointerLevel == 0 && (getExprType(node.left.get()).type == "int")) {
                    emit("asw .ax");
                } else {
                    for (int i = 0; i < shifts; i++) {
                        emitter->asl_a(); emitter->pha(); emitter->txa(); emitter->rol_a(); emitter->tax(); emitter->pla();
                    }
                }
            } else {
                int zpIdx = allocateZP(2);
                std::stringstream ss; ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpIdx);
                emit("stax $" + ss.str());
                std::stringstream ssVal; ssVal << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << val;
                emit("ldax #$" + ssVal.str());
                emit("mul.16 .ax, $" + ss.str());
                freeZP(zpIdx, 2);
            }
            invalidateFlags(); resultNeeded = oldNeeded; return;
        } else if (node.op == "/" || node.op == "%") {
            if (val > 0 && (val & (val - 1)) == 0) {
                if (node.op == "/") {
                    int shifts = 0; while (val > 1) { val >>= 1; shifts++; }
                    for (int i = 0; i < shifts; i++) {
                        emitter->txa(); emitter->lsr_a(); emitter->tax(); emitter->pla(); emitter->ror_a(); emitter->pha();
                    }
                } else { // %
                    int mask = val - 1;
                    emitter->and_imm(mask & 0xFF); emitter->pha(); emitter->txa(); emitter->and_imm((mask >> 8) & 0xFF); emitter->tax(); emitter->pla();
                }
            } else {
                int zpIdx = allocateZP(2);
                std::stringstream ss; ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpIdx);
                emit("stax $" + ss.str());
                std::stringstream ss2; 
                ss2 << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << val;
                emit("ldax #$" + ss2.str());
                if (node.op == "/") emit("div.16 .ax, $" + ss.str());
                else { emit("div.16 .ax, $" + ss.str()); emit("lda $D770"); emit("ldx $D771"); }
                freeZP(zpIdx, 2);
            }
            invalidateFlags(); resultNeeded = oldNeeded; return;
        } else if (node.op == "<<" || node.op == ">>") {
            for (int i = 0; i < val; i++) {
                if (node.op == "<<") { emitter->asl_a(); emitter->pha(); emitter->txa(); emitter->rol_a(); emitter->tax(); emitter->pla(); }
                else { emitter->txa(); emitter->lsr_a(); emitter->tax(); emitter->pla(); emitter->ror_a(); emitter->pha(); }
            }
            invalidateFlags(); resultNeeded = oldNeeded; return;
        }
    }

    int zpIdx = allocateZP(2);
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpIdx);
    emit("stax $" + ss.str());
    node.right->accept(*this);
    resultNeeded = oldNeeded;
    if (scale > 1) {
        emitter->asl_a(); emitter->pha(); emitter->txa(); emitter->rol_a(); emitter->tax(); emitter->pla();
    }
    if (node.op == "+") {
        std::stringstream ss2; ss2 << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpIdx);
        emit("add.16 .ax, $" + ss2.str());
    } else if (node.op == "-") {
        std::stringstream ss2; ss2 << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpIdx);
        emit("sub.16 .ax, $" + ss2.str());
    } else if (node.op == "*") {
        std::stringstream ss2; ss2 << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpIdx);
        emit("mul.16 .ax, $" + ss2.str());
    } else if (node.op == "/" || node.op == "%") {
        std::stringstream ss2; ss2 << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpIdx);
        emit("div.16 .ax, $" + ss2.str());
        if (node.op == "%") { emit("lda $D770"); emit("ldx $D771"); }
    } else if (node.op == "<<") {
        std::string labelStart = newDontCareLabel(); std::string labelEnd = newDontCareLabel();
        int shiftZp = allocateZP(1); std::stringstream ssSh; ssSh << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(shiftZp);
        emit("sta $" + ssSh.str()); std::stringstream ssVal; ssVal << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpIdx);
        emit("ldax $" + ssVal.str()); out << labelStart << ":" << std::endl;
        emit("lda $" + ssSh.str()); emit("beq " + labelEnd); emit("dec $" + ssSh.str());
        emitter->asl_a(); emitter->pha(); emitter->txa(); emitter->rol_a(); emitter->tax(); emitter->pla();
        emit("bra " + labelStart); out << labelEnd << ":" << std::endl;
        freeZP(shiftZp, 1);
    } else if (node.op == ">>") {
        std::string labelStart = newDontCareLabel(); std::string labelEnd = newDontCareLabel();
        int shiftZp = allocateZP(1); std::stringstream ssSh; ssSh << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(shiftZp);
        emit("sta $" + ssSh.str()); std::stringstream ssVal; ssVal << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpIdx);
        emit("ldax $" + ssVal.str()); out << labelStart << ":" << std::endl;
        emit("lda $" + ssSh.str()); emit("beq " + labelEnd); emit("dec $" + ssSh.str());
        emitter->txa(); emitter->lsr_a(); emitter->tax(); emitter->pla(); emitter->ror_a(); emitter->pha();
        emit("bra " + labelStart); out << labelEnd << ":" << std::endl;
        freeZP(shiftZp, 1);
    } else if (node.op == "&") {
        std::stringstream ss2; ss2 << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpIdx);
        emit("and.16 .ax, $" + ss2.str());
    } else if (node.op == "|") {
        std::stringstream ss2; ss2 << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpIdx);
        emit("ora.16 .ax, $" + ss2.str());
    } else if (node.op == "^") {
        std::stringstream ss2; ss2 << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpIdx);
        emit("eor.16 .ax, $" + ss2.str());
    } else if (node.op == "==" || node.op == "!=") {
        std::stringstream ss2; ss2 << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpIdx);
        emit("cpw .ax, $" + ss2.str());
        std::string labelFalse = newDontCareLabel(); std::string labelEnd = newDontCareLabel();
        if (node.op == "==") emit("bne " + labelFalse); else emit("beq " + labelFalse);
        emitter->lda_imm(1); emitter->bra(0x02); out << labelFalse << ":" << std::endl; emitter->lda_imm(0); out << labelEnd << ":" << std::endl;
        emitter->ldx_imm(0); updateRegX(0);
    }
    invalidateFlags();
    freeZP(zpIdx, 2);
    invalidateRegs();
}

void CodeGenerator::visit(ConditionalExpression& node) {
    embedSource(node);
    std::string labelElse = newLabel();
    std::string labelEnd = newLabel();
    emitJumpIfFalse(node.condition.get(), labelElse);
    node.thenExpr->accept(*this);
    emit("bra " + labelEnd);
    out << labelElse << ":" << std::endl;
    invalidateRegs();
    node.elseExpr->accept(*this);
    out << labelEnd << ":" << std::endl;
    invalidateRegs();
}

void CodeGenerator::visit(ArrayAccess& node) {
    embedSource(node);
    if (!resultNeeded) {
        node.arrayExpr->accept(*this);
        node.indexExpr->accept(*this);
        return;
    }
    emitAddress(&node); // Get element address in AX
    int zpAddr = allocateZP(2);
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpAddr);
    emit("stax $" + ss.str());
    
    ExpressionType resType = getExprType(&node);
    if (resType.pointerLevel > 0 || resType.type == "int") {
        std::stringstream ss2;
        ss2 << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpAddr);
        emit("ptrderef $" + ss2.str());
    } else {
        emitter->lda_ind_z(emitter->getZP(zpAddr), false);
        updateRegY(0); emitter->ldx_imm(0); updateRegX(0);
    }
    freeZP(zpAddr, 2);
    invalidateRegs();
}

void CodeGenerator::visit(UnaryOperation& node) {
    if (node.op == "*") {
        bool oldNeeded = resultNeeded;
        resultNeeded = true;
        node.operand->accept(*this);
        resultNeeded = oldNeeded;
        if (!resultNeeded) { invalidateRegs(); return; }
        int zpIdx = allocateZP(2);
        std::stringstream ss;
        ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpIdx);
        emit("stax $" + ss.str());
        ExpressionType subType = getExprType(node.operand.get());
        if (subType.type == "char" && subType.pointerLevel == 1) {
            emitter->lda_ind_z(emitter->getZP(zpIdx), false);
            updateRegY(0); emitter->ldx_imm(0); updateRegX(0);
        } else {
            std::stringstream ss2;
            ss2 << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpIdx);
            emit("ptrderef $" + ss2.str());
        }
        freeZP(zpIdx, 2);
    } else if (node.op == "&") {
        emitAddress(node.operand.get());
    } else if (node.op == "++" || node.op == "--" || node.op == "++_POST" || node.op == "--_POST") {
        bool isInc = (node.op.substr(0, 2) == "++");
        bool isPost = (node.op.find("_POST") != std::string::npos);
        if (auto* ref = dynamic_cast<VariableReference*>(node.operand.get())) {
            std::string rName = resolveVarName(ref->name);
            VarInfo vi = variableTypes.count(rName) ? variableTypes.at(rName) : globalVariableTypes.at(rName);
            bool is16Bit = (vi.pointerLevel > 0 || vi.type == "int");
            if (isStruct(vi.type)) {
                std::string sName = getAggregateName(vi.type);
                if (structs.count(sName) && structs[sName]->totalSize > 1) is16Bit = true;
            }
            if (isPost && resultNeeded) ref->accept(*this);
            invalidateRegs();
            if (is16Bit) {
                if (isInc) emit("inw " + rName + ", s");
                else emit("dew " + rName + ", s");
            } else {
                if (isInc) emit("inc " + rName + ", s");
                else emit("dec " + rName + ", s");
            }
            if (!isPost && resultNeeded) ref->accept(*this);
        } else if (auto* ma = dynamic_cast<MemberAccess*>(node.operand.get())) {
            if (!ma->isArrow) {
                if (auto* ref = dynamic_cast<VariableReference*>(ma->structExpr.get())) {
                    std::string rName = resolveVarName(ref->name);
                    ExpressionType baseType = getExprType(ref);
                    if (isStruct(baseType.type)) {
                        std::string sName = getAggregateName(baseType.type);
                        if (structs.count(sName) && structs[sName]->members.count(ma->memberName)) {
                            MemberInfo& mInfo = structs[sName]->members[ma->memberName];
                            bool is16Bit = (mInfo.pointerLevel > 0 || mInfo.type == "int");
                            if (isStruct(mInfo.type)) {
                                std::string nestedSName = getAggregateName(mInfo.type);
                                if (structs.count(nestedSName) && structs[nestedSName]->totalSize > 1) is16Bit = true;
                            }
                            if (isPost && resultNeeded) node.operand->accept(*this);
                            invalidateRegs();
                            if (is16Bit) {
                                if (isInc) emit("inw " + rName + "+" + std::to_string(mInfo.offset) + ", s");
                                else emit("dew " + rName + "+" + std::to_string(mInfo.offset) + ", s");
                            } else {
                                if (isInc) emit("inc " + rName + "+" + std::to_string(mInfo.offset) + ", s");
                                else emit("dec " + rName + "+" + std::to_string(mInfo.offset) + ", s");
                            }
                            if (!isPost && resultNeeded) node.operand->accept(*this);
                        }
                    }
                }
            } else {
                bool oldNeeded = resultNeeded; resultNeeded = true; node.operand->accept(*this); resultNeeded = oldNeeded; invalidateRegs();
            }
        } else {
            bool oldNeeded = resultNeeded; resultNeeded = true; node.operand->accept(*this); resultNeeded = oldNeeded; invalidateRegs();
        }
    } else {
        bool oldNeeded = resultNeeded; resultNeeded = true; node.operand->accept(*this); resultNeeded = oldNeeded;
        if (node.op == "!") {
            ExpressionType subType = getExprType(node.operand.get());
            bool is8Bit = (subType.type == "char" && subType.pointerLevel == 0);
            if (is8Bit) emit("chkzero.8"); else emit("chkzero.16");
            invalidateRegs(); updateZNFlags(FlagSource::A); emitter->ldx_imm(0); updateRegX(0);
        } else if (node.op == "~") { emitter->not_16(); }
        else if (node.op == "-") { emitter->neg_16(); }
    }
    invalidateRegs();
}

void CodeGenerator::visit(AsmStatement& node) {
    embedSource(node);
    emit(node.code);
    invalidateRegs();
}

void CodeGenerator::visit(StaticAssert& node) {}

void CodeGenerator::visit(ReturnStatement& node) {
    embedSource(node);
    if (node.expression) {
        bool oldNeeded = resultNeeded;
        resultNeeded = true;
        node.expression->accept(*this);
        resultNeeded = oldNeeded;
    }
    emit("rtn #$00");
    invalidateRegs();
}

void CodeGenerator::visit(BreakStatement& node) {
    embedSource(node);
    if (!loopStack.empty()) emit("bra " + loopStack.back().breakLabel);
}

void CodeGenerator::visit(ContinueStatement& node) {
    embedSource(node);
    if (!loopStack.empty() && !loopStack.back().continueLabel.empty()) emit("bra " + loopStack.back().continueLabel);
}

void CodeGenerator::visit(SwitchContinueStatement& node) {
    embedSource(node);
    if (switchStack.empty()) return;
    SwitchInfo* info = switchStack.back();
    if (!node.target) {
        if (info->hasDefault) emit("bra " + info->defaultLabel);
    } else {
        if (auto* lit = dynamic_cast<IntegerLiteral*>(node.target.get())) {
            for (auto& c : info->cases) if (c.value == (uint32_t)lit->value) { emit("bra " + c.label); break; }
        }
    }
}

void CodeGenerator::visit(ExpressionStatement& node) {
    embedSource(node);
    bool oldNeeded = resultNeeded;
    resultNeeded = false;
    node.expression->accept(*this);
    resultNeeded = oldNeeded;
}

void CodeGenerator::visit(IfStatement& node) {
    embedSource(node);
    
    if (auto* lit = dynamic_cast<IntegerLiteral*>(node.condition.get())) {
        if (lit->value != 0) {
            node.thenBranch->accept(*this);
        } else if (node.elseBranch) {
            node.elseBranch->accept(*this);
        }
        return;
    }

    std::string labelElse = newLabel();
    std::string labelEnd = newLabel();
    emitJumpIfFalse(node.condition.get(), labelElse);
    node.thenBranch->accept(*this);
    if (node.elseBranch) {
        emit("bra " + labelEnd);
        out << labelElse << ":" << std::endl;
        invalidateRegs();
        node.elseBranch->accept(*this);
        out << labelEnd << ":" << std::endl;
    } else {
        out << labelElse << ":" << std::endl;
    }
    invalidateRegs();
}

void CodeGenerator::visit(WhileStatement& node) {
    embedSource(node);
    
    // Optimization for while(1) or while(constant)
    if (auto* lit = dynamic_cast<IntegerLiteral*>(node.condition.get())) {
        if (lit->value != 0) {
            std::string labelStart = newLabel();
            std::string labelEnd = newLabel();
            out << labelStart << ":" << std::endl;
            invalidateRegs();
            loopStack.push_back({labelStart, labelEnd});
            node.body->accept(*this);
            loopStack.pop_back();
            emit("bra " + labelStart);
            out << labelEnd << ":" << std::endl;
            invalidateRegs();
            return;
        } else {
            return;
        }
    }

    std::string labelStart = newLabel();
    std::string labelEnd = newLabel();
    out << labelStart << ":" << std::endl;
    invalidateRegs();
    emitJumpIfFalse(node.condition.get(), labelEnd);
    loopStack.push_back({labelStart, labelEnd});
    node.body->accept(*this);
    loopStack.pop_back();
    emit("bra " + labelStart);
    out << labelEnd << ":" << std::endl;
    invalidateRegs();
}

void CodeGenerator::visit(DoWhileStatement& node) {
    embedSource(node);
    std::string labelStart = newLabel();
    std::string labelCondition = newLabel();
    std::string labelEnd = newLabel();
    out << labelStart << ":" << std::endl;
    invalidateRegs();
    loopStack.push_back({labelCondition, labelEnd});
    node.body->accept(*this);
    loopStack.pop_back();
    out << labelCondition << ":" << std::endl;
    emitJumpIfTrue(node.condition.get(), labelStart);
    out << labelEnd << ":" << std::endl;
    invalidateRegs();
}

void CodeGenerator::visit(ForStatement& node) {
    embedSource(node);
    if (node.initializer) {
        bool oldNeeded = resultNeeded;
        resultNeeded = false;
        node.initializer->accept(*this);
        resultNeeded = oldNeeded;
    }
    std::string labelStart = newLabel();
    std::string labelIncrement = newLabel();
    std::string labelEnd = newLabel();
    out << labelStart << ":" << std::endl;
    if (node.condition) emitJumpIfFalse(node.condition.get(), labelEnd);
    loopStack.push_back({labelIncrement, labelEnd});
    node.body->accept(*this);
    loopStack.pop_back();
    out << labelIncrement << ":" << std::endl;
    if (node.increment) {
        bool oldNeeded = resultNeeded;
        resultNeeded = false;
        node.increment->accept(*this);
        resultNeeded = oldNeeded;
    }
    emit("bra " + labelStart);
    out << labelEnd << ":" << std::endl;
    invalidateRegs();
}

void CodeGenerator::visit(SwitchStatement& node) {
    embedSource(node);
    bool oldNeeded = resultNeeded;
    resultNeeded = true;
    node.expression->accept(*this);
    resultNeeded = oldNeeded;
    int zpExpr = allocateZP(2);
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpExpr);
    emit("sta $" + ss.str()); emit("txa");
    std::stringstream ssHigh;
    ssHigh << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpExpr) + 1;
    emit("sta $" + ssHigh.str());
    invalidateRegs();
    std::string labelBreak = newLabel();
    SwitchInfo info;
    info.zpExpr = zpExpr;
    info.breakLabel = labelBreak;
    switchStack.push_back(&info);
    loopStack.push_back({"", labelBreak});
    class CaseCollector : public ASTVisitor {
    public:
        SwitchInfo& info;
        CodeGenerator& gen;
        CaseCollector(SwitchInfo& i, CodeGenerator& g) : info(i), gen(g) {}
        void visit(IntegerLiteral&) override {}
        void visit(StringLiteral&) override {}
        void visit(VariableReference&) override {}
        void visit(Assignment& node) override { node.expression->accept(*this); }
        void visit(BinaryOperation& node) override { node.left->accept(*this); node.right->accept(*this); }
        void visit(UnaryOperation& node) override { node.operand->accept(*this); }
        void visit(FunctionCall& node) override { for (auto& arg : node.arguments) arg->accept(*this); }
        void visit(MemberAccess& node) override { node.structExpr->accept(*this); }
        void visit(ConditionalExpression& node) override { node.condition->accept(*this); node.thenExpr->accept(*this); node.elseExpr->accept(*this); }
        void visit(ArrayAccess& node) override { node.arrayExpr->accept(*this); node.indexExpr->accept(*this); }
        void visit(AlignofExpression&) override {}
        void visit(VariableDeclaration& node) override { if (node.initializer) node.initializer->accept(*this); }
        void visit(ReturnStatement& node) override { if(node.expression) node.expression->accept(*this); }
        void visit(BreakStatement&) override {}
        void visit(ContinueStatement&) override {}
        void visit(SwitchContinueStatement&) override {}
        void visit(ExpressionStatement& node) override { node.expression->accept(*this); }
        void visit(IfStatement& node) override { node.condition->accept(*this); node.thenBranch->accept(*this); if(node.elseBranch) node.elseBranch->accept(*this); }
        void visit(WhileStatement& node) override { node.condition->accept(*this); node.body->accept(*this); }
        void visit(DoWhileStatement& node) override { node.body->accept(*this); node.condition->accept(*this); }
        void visit(ForStatement& node) override { if(node.initializer) node.initializer->accept(*this); if(node.condition) node.condition->accept(*this); if(node.increment) node.increment->accept(*this); node.body->accept(*this); }
        void visit(SwitchStatement&) override {}
        void visit(CaseStatement& node) override {
            uint32_t val = 0; if (auto* lit = dynamic_cast<IntegerLiteral*>(node.value.get())) val = lit->value;
            std::string l = gen.newLabel(); info.cases.push_back({val, l}); node.label = l;
        }
        void visit(DefaultStatement& node) override {
            std::string l = gen.newLabel(); info.defaultLabel = l; info.hasDefault = true; node.label = l;
        }
        void visit(AsmStatement&) override {}
        void visit(StaticAssert&) override {}
        void visit(StructDefinition&) override {}
        void visit(CompoundStatement& node) override { for(auto& s : node.statements) s->accept(*this); }
        void visit(FunctionDeclaration&) override {}
        void visit(TranslationUnit&) override {}
    };
    CaseCollector collector(info, *this);
    node.body->accept(collector);
    for (auto& c : info.cases) {
        emit("ldax $" + ss.str());
        std::stringstream ssVal; ssVal << "#$" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << (uint16_t)c.value;
        emit("cpw .ax, " + ssVal.str()); emit("beq " + c.label);
    }
    if (info.hasDefault) emit("bra " + info.defaultLabel);
    else emit("bra " + labelBreak);
    node.body->accept(*this);
    out << labelBreak << ":" << std::endl;
    invalidateRegs();
    freeZP(zpExpr, 2);
    switchStack.pop_back();
    loopStack.pop_back();
}

void CodeGenerator::visit(CaseStatement& node) {
    if (!node.label.empty()) { out << node.label << ":" << std::endl; invalidateRegs(); }
}

void CodeGenerator::visit(DefaultStatement& node) {
    if (!node.label.empty()) { out << node.label << ":" << std::endl; invalidateRegs(); }
}

void CodeGenerator::visit(StructDefinition& node) {
    auto info = std::make_shared<StructInfo>();
    info->name = node.name;
    int currentOffset = 0;
    int maxAlign = 1;
    int maxSize = 0;

    for (auto& member : node.members) {
        int mAlign = 1;
        int mSize = 0;

        if (member.pointerLevel > 0 || member.type == "int") {
            mAlign = 2;
            mSize = 2;
        } else if (member.type == "char") {
            mAlign = 1;
            mSize = 1;
        } else if (isStruct(member.type)) {
            std::string sName = getAggregateName(member.type);
            if (structs.count(sName)) {
                mAlign = structs[sName]->alignment;
                mSize = structs[sName]->totalSize;
            } else {
                throw std::runtime_error("Unknown struct/union type in member: " + sName);
            }
        }

        if (member.alignment > mAlign) mAlign = member.alignment;
        if (mAlign > maxAlign) maxAlign = mAlign;
        
        if (member.arraySize >= 0) mSize *= member.arraySize;

        if (!node.isUnion) {
            if (currentOffset % mAlign != 0) currentOffset += mAlign - (currentOffset % mAlign);
        }

        if (member.isAnonymous && isStruct(member.type)) {
            std::string nestedSName = getAggregateName(member.type);
            if (structs.count(nestedSName)) {
                auto& nestedInfo = *structs[nestedSName];
                for (auto const& [mName, mInfo] : nestedInfo.members) {
                    MemberInfo flattenedInfo = mInfo;
                    flattenedInfo.offset += node.isUnion ? 0 : currentOffset;
                    info->members[mName] = flattenedInfo;
                }
                if (!node.isUnion) currentOffset += mSize;
                else if (mSize > maxSize) maxSize = mSize;
            }
        } else {
            MemberInfo mInfo;
            mInfo.type = member.type;
            mInfo.pointerLevel = member.pointerLevel;
            mInfo.offset = node.isUnion ? 0 : currentOffset;
            mInfo.alignment = mAlign;
            mInfo.arraySize = member.arraySize;
            if (!member.name.empty()) info->members[member.name] = mInfo;
            if (!node.isUnion) currentOffset += mSize;
            else if (mSize > maxSize) maxSize = mSize;
        }
    }
    if (node.isUnion) currentOffset = maxSize;
    if (currentOffset % maxAlign != 0) currentOffset += maxAlign - (currentOffset % maxAlign);
    info->totalSize = currentOffset;
    info->alignment = maxAlign;
    structs[node.name] = info;
}

void CodeGenerator::visit(MemberAccess& node) {
    ExpressionType baseType = getExprType(node.structExpr.get());
    if (!isStruct(baseType.type)) {
         if (auto* ref = dynamic_cast<VariableReference*>(node.structExpr.get())) {
             if (globalVariableTypes.count("_g_" + ref->name)) {
                 baseType = {globalVariableTypes.at("_g_" + ref->name).type, globalVariableTypes.at("_g_" + ref->name).pointerLevel};
             }
         }
    }
    if (!isStruct(baseType.type)) throw std::runtime_error("Dot/Arrow operator on non-struct type: " + baseType.type);
    std::string sName = getAggregateName(baseType.type);
    if (!structs.count(sName)) throw std::runtime_error("Unknown struct/union type: " + sName);
    auto& sInfo = *structs[sName];
    if (!sInfo.members.count(node.memberName)) throw std::runtime_error("Member " + node.memberName + " not found in struct/union " + sName);
    MemberInfo& mInfo = sInfo.members[node.memberName];

    if (!node.isArrow) {
        if (auto* ref = dynamic_cast<VariableReference*>(node.structExpr.get())) {
            std::string rName = resolveVarName(ref->name);
            embedSource(node);
            if (!resultNeeded) return;
            bool is16 = (mInfo.pointerLevel > 0 || mInfo.type == "int");
            if (isStruct(mInfo.type)) {
                std::string nestedSName = getAggregateName(mInfo.type);
                if (structs.count(nestedSName) && structs[nestedSName]->totalSize > 1) is16 = true;
            }
            if (is16) {
                emit("ldax " + rName + "+" + std::to_string(mInfo.offset) + ", s");
                updateRegAVar(rName, mInfo.offset); updateRegXVar(rName, mInfo.offset + 1);
                updateZNFlags(FlagSource::A);
            } else {
                emit("lda " + rName + "+" + std::to_string(mInfo.offset) + ", s");
                updateRegAVar(rName, mInfo.offset);
                emitter->ldx_imm(0); updateRegX(0);
            }
            return;
        }
    }

    if (node.isArrow) {
        bool oldNeeded = resultNeeded;
        resultNeeded = true;
        node.structExpr->accept(*this);
        resultNeeded = oldNeeded;
    } else {
        emitAddress(node.structExpr.get());
    }
    if (!resultNeeded) { invalidateRegs(); return; }

    if (mInfo.offset > 0) emitter->add_16_imm(mInfo.offset);
    int zpIdx = allocateZP(2);
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpIdx);
    emit("stax $" + ss.str());
    emitter->lda_ind_z(emitter->getZP(zpIdx), false);
    updateRegY(0); regA.known = false;
    bool is16 = (mInfo.pointerLevel > 0 || mInfo.type == "int");
    if (isStruct(mInfo.type)) {
        std::string nestedSName = getAggregateName(mInfo.type);
        if (structs.count(nestedSName) && structs[nestedSName]->totalSize > 1) is16 = true;
    }
    if (is16) {
        emitter->pha(); emitter->ldy_imm(1); updateRegY(1);
        std::stringstream ss2;
        ss2 << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpIdx);
        emit("lda ($" + ss2.str() + "),y");
        regA.known = false; emitter->tax(); emitter->pla();
    } else {
        emitter->ldx_imm(0); updateRegX(0);
    }
    freeZP(zpIdx, 2);
    invalidateRegs();
}

void CodeGenerator::visit(IntegerLiteral& node) {
    if (!resultNeeded) return;
    if (node.value == 0) {
        emit("zero a, x"); updateRegA(0); updateRegX(0); updateZNFlags(FlagSource::A);
        return;
    }
    std::stringstream ss;
    ss << "#$" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << (int)node.value;
    emit("ldax " + ss.str());
    updateRegA(node.value & 0xFF); updateRegX((node.value >> 8) & 0xFF); updateZNFlags(FlagSource::A);
}

void CodeGenerator::visit(StringLiteral& node) {
    if (!resultNeeded) return;
    if (stringPool.find(node.value) == stringPool.end()) stringPool[node.value] = "STR" + std::to_string(stringCount++);
    std::string label = stringPool[node.value];
    emit("ldax #<" + label); invalidateRegs();
}

void CodeGenerator::visit(VariableReference& node) {
    if (!resultNeeded) return;
    std::string rName = resolveVarName(node.name);
    bool isGlobal = (rName.length() >= 3 && rName.substr(0, 3) == "_g_");
    std::string suffix = isGlobal ? "" : ", s";
    VarInfo vi = variableTypes.count(rName) ? variableTypes.at(rName) : globalVariableTypes.at(rName);

    if (vi.arraySize >= 0) {
        emitAddress(&node); // This will put address in AX
        return;
    }

    bool is16Bit = (vi.pointerLevel > 0 || vi.type == "int");
    if (isStruct(vi.type)) {
        std::string sName = getAggregateName(vi.type);
        if (structs.count(sName) && structs[sName]->totalSize > 1) is16Bit = true;
    }
    if (is16Bit) {
        bool lowCorrect = (regA.known && regA.isVariable && regA.varName == rName && regA.varOffset == 0);
        bool highCorrect = (regX.known && regX.isVariable && regX.varName == rName && regX.varOffset == 1);
        if (lowCorrect && highCorrect) {}
        else if (lowCorrect) { emit("ldx " + rName + "+1" + suffix); updateRegXVar(rName, 1); }
        else if (highCorrect) { emit("lda " + rName + suffix); updateRegAVar(rName, 0); }
        else {
            emit("ldax " + rName + suffix); updateRegAVar(rName, 0); updateRegXVar(rName, 1); updateZNFlags(FlagSource::A);
        }
    } else {
        if (regA.known && regA.isVariable && regA.varName == rName && regA.varOffset == 0) {}
        else if (regX.known && regX.isVariable && regX.varName == rName && regX.varOffset == 0) {
            emit("txa"); transferRegs(FlagSource::A, FlagSource::X);
        } else if (regY.known && regY.isVariable && regY.varName == rName && regY.varOffset == 0) {
            emit("tya"); transferRegs(FlagSource::A, FlagSource::Y);
        } else if (regZ.known && regZ.isVariable && regZ.varName == rName && regZ.varOffset == 0) {
            emit("tza"); transferRegs(FlagSource::A, FlagSource::Z);
        } else {
            emit("lda " + rName + suffix); updateRegAVar(rName, 0);
        }
        if (!regX.known || regX.isVariable || regX.value != 0) { emitter->ldx_imm(0); updateRegX(0); }
    }
}

void CodeGenerator::visit(FunctionCall& node) {
    if (node.name == "_memset" && node.arguments.size() == 3) {
        bool oldNeeded = resultNeeded; resultNeeded = true;
        node.arguments[0]->accept(*this);
        int zpDest = allocateZP(2);
        std::stringstream ssDest; ssDest << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpDest);
        emit("stax $" + ssDest.str());
        node.arguments[1]->accept(*this);
        if (auto* lit = dynamic_cast<IntegerLiteral*>(node.arguments[2].get())) {
            emit("FILL ($" + ssDest.str() + "), #" + std::to_string(lit->value));
        } else {
            node.arguments[2]->accept(*this);
            emit("taz"); emit("txa"); emit("tay"); emit("tza"); emit("tax");
            emit("FILL ($" + ssDest.str() + "), .XY");
        }
        freeZP(zpDest, 2); resultNeeded = oldNeeded; invalidateRegs(); return;
    }
    if (node.name == "_memcpy" && node.arguments.size() == 3) {
        bool oldNeeded = resultNeeded; resultNeeded = true;
        node.arguments[0]->accept(*this);
        int zpDest = allocateZP(2);
        std::stringstream ssDest; ssDest << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpDest);
        emit("stax $" + ssDest.str());
        node.arguments[1]->accept(*this);
        int zpSrc = allocateZP(2);
        std::stringstream ssSrc; ssSrc << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)emitter->getZP(zpSrc);
        emit("stax $" + ssSrc.str());
        if (auto* lit = dynamic_cast<IntegerLiteral*>(node.arguments[2].get())) {
            std::stringstream ssLenX, ssLenY;
            ssLenX << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (lit->value & 0xFF);
            ssLenY << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << ((lit->value >> 8) & 0xFF);
            emit("ldx #$" + ssLenX.str()); emit("ldy #$" + ssLenY.str());
        } else {
            node.arguments[2]->accept(*this);
            emit("taz"); emit("txa"); emit("tay"); emit("tza"); emit("tax");
        }
        emit("MOVE ($" + ssSrc.str() + "), ($" + ssDest.str() + ")");
        freeZP(zpSrc, 2); freeZP(zpDest, 2); resultNeeded = oldNeeded; invalidateRegs(); return;
    }
    bool oldNeeded = resultNeeded; resultNeeded = true;
    for (auto& arg : node.arguments) {
        if (auto* ref = dynamic_cast<VariableReference*>(arg.get())) {
            std::string rName = resolveVarName(ref->name);
            VarInfo vi = variableTypes.count(rName) ? variableTypes.at(rName) : globalVariableTypes.at(rName);
            bool is16Bit = (vi.pointerLevel > 0 || vi.type == "int");
            if (isStruct(vi.type)) {
                std::string sName = getAggregateName(vi.type);
                if (structs.count(sName) && structs[sName]->totalSize > 1) is16Bit = true;
            }
            if (is16Bit) emit("phw.s " + rName + ", s");
            else { arg->accept(*this); emitter->pha(); }
        } else { arg->accept(*this); emitter->push_ax(); }
    }
    resultNeeded = oldNeeded; emit("jsr " + node.name);
    if (node.arguments.size() > 0) emit("rtn #" + std::to_string(node.arguments.size() * 2));
    invalidateRegs();
}

void CodeGenerator::emitJumpIfTrue(Expression* cond, const std::string& labelTrue) {
    if (auto* lit = dynamic_cast<IntegerLiteral*>(cond)) {
        if (lit->value != 0) {
            emit("bra " + labelTrue);
        }
        return;
    }
    if (auto* bin = dynamic_cast<BinaryOperation*>(cond)) {
        if (bin->op == "||") { emitJumpIfTrue(bin->left.get(), labelTrue); emitJumpIfTrue(bin->right.get(), labelTrue); return; }
        if (bin->op == "&&") {
            std::string labelFalse = newDontCareLabel(); emitJumpIfFalse(bin->left.get(), labelFalse);
            emitJumpIfTrue(bin->right.get(), labelTrue); out << labelFalse << ":" << std::endl; return;
        }
        if (bin->op == "==" || bin->op == "!=" || bin->op == "<" || bin->op == ">" || bin->op == "<=" || bin->op == ">=") {
            bool oldNeeded = resultNeeded; resultNeeded = true; cond->accept(*this); resultNeeded = oldNeeded;
            if (flags.znSource != FlagSource::A) emit("cmp #$00");
            emit("bne " + labelTrue); return;
        }
    }
    if (auto* un = dynamic_cast<UnaryOperation*>(cond)) if (un->op == "!") { emitJumpIfFalse(un->operand.get(), labelTrue); return; }
    ExpressionType condType = getExprType(cond); bool is8Bit = (condType.type == "char" && condType.pointerLevel == 0);
    bool oldNeeded = resultNeeded; resultNeeded = true; cond->accept(*this); resultNeeded = oldNeeded;
    if (is8Bit) { if (flags.znSource != FlagSource::A) emit("cmp #$00"); emit("bne " + labelTrue); }
    else { if (flags.znSource != FlagSource::A) emit("cmp #$00"); emit("branch.16 bne, " + labelTrue); }
}

void CodeGenerator::emitJumpIfFalse(Expression* cond, const std::string& labelElse) {
    if (auto* lit = dynamic_cast<IntegerLiteral*>(cond)) {
        if (lit->value == 0) {
            emit("bra " + labelElse);
        }
        return;
    }
    if (auto* bin = dynamic_cast<BinaryOperation*>(cond)) {
        if (bin->op == "||") {
            std::string labelThen = newDontCareLabel(); emitJumpIfTrue(bin->left.get(), labelThen);
            emitJumpIfFalse(bin->right.get(), labelElse); out << labelThen << ":" << std::endl; return;
        }
        if (bin->op == "&&") { emitJumpIfFalse(bin->left.get(), labelElse); emitJumpIfFalse(bin->right.get(), labelElse); return; }
        if (bin->op == "==" || bin->op == "!=" || bin->op == "<" || bin->op == ">" || bin->op == "<=" || bin->op == ">=") {
            bool oldNeeded = resultNeeded; resultNeeded = true; cond->accept(*this); resultNeeded = oldNeeded;
            if (flags.znSource != FlagSource::A) emit("cmp #$00");
            emit("beq " + labelElse); return;
        }
    }
    if (auto* un = dynamic_cast<UnaryOperation*>(cond)) if (un->op == "!") { emitJumpIfTrue(un->operand.get(), labelElse); return; }
    ExpressionType condType = getExprType(cond); bool is8Bit = (condType.type == "char" && condType.pointerLevel == 0);
    bool oldNeeded = resultNeeded; resultNeeded = true; cond->accept(*this); resultNeeded = oldNeeded;
    if (is8Bit) { if (flags.znSource != FlagSource::A) emit("cmp #$00"); emit("beq " + labelElse); }
    else { if (flags.znSource != FlagSource::A) emit("cmp #$00"); emit("branch.16 beq, " + labelElse); }
}

void CodeGenerator::visit(AlignofExpression& node) {
    int alignment = 1;
    if (node.pointerLevel > 0 || node.typeName == "int") alignment = 2;
    else if (node.typeName == "char") alignment = 1;
    else { std::string sName = getAggregateName(node.typeName); if (structs.count(sName)) alignment = structs[sName]->alignment; }
    emitter->lda_imm(alignment & 0xFF); updateRegA(alignment & 0xFF); invalidateFlags();
}

void CodeGenerator::emitData() {
    out << std::endl << ".data" << std::endl;
    out << "; Data Section" << std::endl;

    std::vector<VariableDeclaration*> uninitializedVars;

    for (auto* gVar : globalVars) {
        if (!gVar->initializer) {
            uninitializedVars.push_back(gVar);
            continue;
        }

        if (gVar->alignment > 1) out << "    .align " << std::to_string(gVar->alignment) << std::endl;
        out << "_g_" << gVar->name << ":" << std::endl;
        int size = 0;
        if (gVar->pointerLevel > 0) size = 2;
        else if (gVar->type == "char") size = 1;
        else if (gVar->type == "int") size = 2;
        else if (isStruct(gVar->type)) { std::string sName = getAggregateName(gVar->type); if (structs.count(sName)) size = structs[sName]->totalSize; }
        
        if (gVar->arraySize >= 0) size *= gVar->arraySize;
        
        if (auto* lit = dynamic_cast<IntegerLiteral*>(gVar->initializer.get())) {
            if (size == 1) out << "    .byte $" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (lit->value & 0xFF) << std::dec << std::endl;
            else if (size == 2) out << "    .word $" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << (lit->value & 0xFFFF) << std::dec << std::endl;
            else out << "    .res " << std::to_string(size) << ", 0" << std::endl;
        } else {
            out << "    .res " << std::to_string(size) << ", 0" << std::endl;
        }
    }

    if (!uninitializedVars.empty()) {
        out << std::endl << ".bss" << std::endl;
        out << "; BSS Section" << std::endl;
        for (auto* gVar : uninitializedVars) {
            if (gVar->alignment > 1) out << "    .align " << std::to_string(gVar->alignment) << std::endl;
            out << "_g_" << gVar->name << ":" << std::endl;
            int size = 0;
            if (gVar->pointerLevel > 0) size = 2;
            else if (gVar->type == "char") size = 1;
            else if (gVar->type == "int") size = 2;
            else if (isStruct(gVar->type)) { std::string sName = getAggregateName(gVar->type); if (structs.count(sName)) size = structs[sName]->totalSize; }
            if (gVar->arraySize >= 0) size *= gVar->arraySize;
            out << "    .res " << std::to_string(size) << std::endl;
        }
    }

    out << std::endl << ".data" << std::endl;
    for (const auto& entry : stringPool) { out << entry.second << ":" << std::endl; out << "    .text \"" << entry.first << "\"" << std::endl; out << "    .byte 0" << std::endl; }
}

void CodeGenerator::invalidateRegs() {
    regA.known = false; regA.isVariable = false; regA.varName = ""; regA.varOffset = 0; regA.value = 0;
    regX.known = false; regX.isVariable = false; regX.varName = ""; regX.varOffset = 0; regX.value = 0;
    regY.known = false; regY.isVariable = false; regY.varName = ""; regY.varOffset = 0; regY.value = 0;
    regZ.known = false; regZ.isVariable = false; regZ.varName = ""; regZ.varOffset = 0; regZ.value = 0;
    invalidateFlags();
}

void CodeGenerator::invalidateVar(const std::string& name) {
    if (regA.known && regA.isVariable && regA.varName == name) regA.known = false;
    if (regX.known && regX.isVariable && regX.varName == name) regX.known = false;
    if (regY.known && regY.isVariable && regY.varName == name) regY.known = false;
    if (regZ.known && regZ.isVariable && regZ.varName == name) regZ.known = false;
}

void CodeGenerator::transferRegs(FlagSource dest, FlagSource src) {
    RegState* s = nullptr; RegState* d = nullptr;
    if (src == FlagSource::A) s = &regA; else if (src == FlagSource::X) s = &regX; else if (src == FlagSource::Y) s = &regY; else if (src == FlagSource::Z) s = &regZ;
    if (dest == FlagSource::A) d = &regA; else if (dest == FlagSource::X) d = &regX; else if (dest == FlagSource::Y) d = &regY; else if (dest == FlagSource::Z) d = &regZ;
    if (s && d) { *d = *s; updateZNFlags(dest, flags.zero, flags.negative); }
}

void CodeGenerator::updateFlags(TriState c, TriState z, TriState n, TriState v) {
    if (c != TriState::UNKNOWN) flags.carry = c;
    if (z != TriState::UNKNOWN) flags.zero = z;
    if (n != TriState::UNKNOWN) flags.negative = n;
    if (v != TriState::UNKNOWN) flags.overflow = v;
    if (z != TriState::UNKNOWN || n != TriState::UNKNOWN) flags.znSource = FlagSource::NONE;
}

void CodeGenerator::updateZNFlags(FlagSource source, TriState z, TriState n) {
    flags.znSource = source; flags.zero = z; flags.negative = n;
}

void CodeGenerator::invalidateFlags() {
    flags.carry = TriState::UNKNOWN; flags.zero = TriState::UNKNOWN; flags.negative = TriState::UNKNOWN; flags.overflow = TriState::UNKNOWN; flags.znSource = FlagSource::NONE;
}

void CodeGenerator::updateRegA(uint8_t val) { 
    regA.known = true; regA.isVariable = false; regA.value = val; 
    updateZNFlags(FlagSource::A, val == 0 ? TriState::SET : TriState::CLEAR, (val & 0x80) ? TriState::SET : TriState::CLEAR);
}
void CodeGenerator::updateRegX(uint8_t val) { 
    regX.known = true; regX.isVariable = false; regX.value = val; 
    updateZNFlags(FlagSource::X, val == 0 ? TriState::SET : TriState::CLEAR, (val & 0x80) ? TriState::SET : TriState::CLEAR);
}
void CodeGenerator::updateRegY(uint8_t val) { 
    regY.known = true; regY.isVariable = false; regY.value = val; 
    updateZNFlags(FlagSource::Y, val == 0 ? TriState::SET : TriState::CLEAR, (val & 0x80) ? TriState::SET : TriState::CLEAR);
}
void CodeGenerator::updateRegZ(uint8_t val) { 
    regZ.known = true; regZ.isVariable = false; regZ.value = val; 
    updateZNFlags(FlagSource::Z, val == 0 ? TriState::SET : TriState::CLEAR, (val & 0x80) ? TriState::SET : TriState::CLEAR);
}
void CodeGenerator::updateRegAVar(const std::string& name, int offset) { regA.known = true; regA.isVariable = true; regA.varName = name; regA.varOffset = offset; updateZNFlags(FlagSource::A); }
void CodeGenerator::updateRegXVar(const std::string& name, int offset) { regX.known = true; regX.isVariable = true; regX.varName = name; regX.varOffset = offset; updateZNFlags(FlagSource::X); }
void CodeGenerator::updateRegYVar(const std::string& name, int offset) { regY.known = true; regY.isVariable = true; regY.varName = name; regY.varOffset = offset; updateZNFlags(FlagSource::Y); }
void CodeGenerator::updateRegZVar(const std::string& name, int offset) { regZ.known = true; regZ.isVariable = true; regZ.varName = name; regZ.varOffset = offset; updateZNFlags(FlagSource::Z); }

int CodeGenerator::allocateZP(int size) {
    for (int i = 0; i <= (int)zpRegs.size() - size; ++i) {
        bool found = true; for (int j = 0; j < size; ++j) if (zpRegs[i + j].inUse) { found = false; break; }
        if (found) { for (int j = 0; j < size; ++j) zpRegs[i + j].inUse = true; return i; }
    }
    int oldSize = zpRegs.size();
    for (int i = 0; i < size; ++i) zpRegs.push_back({true});
    return oldSize;
}

void CodeGenerator::freeZP(int index, int size) {
    for (int i = 0; i < size; ++i) zpRegs[index + i].inUse = false;
}

class VariableUseChecker : public ASTVisitor {
public:
    std::string targetVarName; bool used = false; std::string currentDeclVarName;
    VariableUseChecker(const std::string& name, const std::string& currentDecl) : targetVarName(name), currentDeclVarName(currentDecl) {}
    void visit(IntegerLiteral&) override {}
    void visit(StringLiteral&) override {}
    void visit(VariableReference& node) override { if (node.name == targetVarName) used = true; }
    void visit(Assignment& node) override { if (auto* ref = dynamic_cast<VariableReference*>(node.target.get())) if (ref->name == targetVarName && ref->name != currentDeclVarName) used = true; node.expression->accept(*this); }
    void visit(BinaryOperation& node) override { node.left->accept(*this); node.right->accept(*this); }
    void visit(UnaryOperation& node) override { node.operand->accept(*this); }
    void visit(FunctionCall& node) override { for (auto& arg : node.arguments) arg->accept(*this); }
    void visit(MemberAccess& node) override { node.structExpr->accept(*this); }
    void visit(ConditionalExpression& node) override { node.condition->accept(*this); node.thenExpr->accept(*this); node.elseExpr->accept(*this); }
    void visit(ArrayAccess& node) override { node.arrayExpr->accept(*this); node.indexExpr->accept(*this); }
    void visit(AlignofExpression&) override {}
    void visit(VariableDeclaration& node) override { if (node.initializer) node.initializer->accept(*this); }
    void visit(ReturnStatement& node) override { if(node.expression) node.expression->accept(*this); }
    void visit(BreakStatement&) override {}
    void visit(ContinueStatement&) override {}
    void visit(SwitchContinueStatement& node) override { if (node.target) node.target->accept(*this); }
    void visit(ExpressionStatement& node) override { if (node.expression) node.expression->accept(*this); }
    void visit(IfStatement& node) override { node.condition->accept(*this); node.thenBranch->accept(*this); if(node.elseBranch) node.elseBranch->accept(*this); }
    void visit(WhileStatement& node) override { node.condition->accept(*this); node.body->accept(*this); }
    void visit(DoWhileStatement& node) override { node.body->accept(*this); node.condition->accept(*this); }
    void visit(ForStatement& node) override { if(node.initializer) node.initializer->accept(*this); if(node.condition) node.condition->accept(*this); if(node.increment) node.increment->accept(*this); node.body->accept(*this); }
    void visit(SwitchStatement& node) override { node.expression->accept(*this); node.body->accept(*this); }
    void visit(CaseStatement& node) override { node.value->accept(*this); }
    void visit(DefaultStatement&) override {}
    void visit(AsmStatement&) override {}
    void visit(StaticAssert&) override {}
    void visit(StructDefinition&) override {}
    void visit(CompoundStatement& node) override { for(auto& s : node.statements) s->accept(*this); }
    void visit(FunctionDeclaration& node) override { node.body->accept(*this); }
    void visit(TranslationUnit& node) override { for(auto& decl : node.topLevelDecls) decl->accept(*this); }
};

bool CodeGenerator::isVariableUsed(const std::string& varName, FunctionDeclaration& func) {
    VariableUseChecker checker(varName, varName);
    bool foundDecl = false;
    for (auto& stmt : func.body->statements) {
        if (!foundDecl) {
            if (auto* varDecl = dynamic_cast<VariableDeclaration*>(stmt.get())) {
                if (varDecl->name == varName) { foundDecl = true; if (varDecl->initializer) varDecl->initializer->accept(checker); continue; }
            }
        }
        if (foundDecl) stmt->accept(checker);
        if (checker.used) return true;
    }
    return checker.used;
}
