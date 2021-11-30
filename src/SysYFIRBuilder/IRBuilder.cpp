#include "IRBuilder.h"

#define CONST_INT(num) ConstantInt::get(num, module.get())
#define CONST_FLOAT(num) ConstantFloat::get(num, module.get())

// You can define global variables and functions here
bool is_global = 1; // at first it is global
// to store state

// store temporary value
Value *tmp_val = nullptr;

// types
Type *VOID_T;
Type *INT1_T;
Type *INT32_T;
Type *FLOAT_T;
Type *INT32PTR_T;
Type *FLOATPTR_T;

void IRBuilder::visit(SyntaxTree::Assembly &node) {
    VOID_T = Type::get_void_type(module.get());
    INT1_T = Type::get_int1_type(module.get());
    INT32_T = Type::get_int32_type(module.get());
    FLOAT_T = Type::get_float_type(module.get());
    INT32PTR_T = Type::get_int32_ptr_type(module.get());
    FLOATPTR_T = Type::get_float_ptr_type(module.get());
    for (const auto &def : node.global_defs) {
        def->accept(*this);
    }
}

// You need to fill them

void IRBuilder::visit(SyntaxTree::InitVal &node) {
    if(node.isExp){
        node.expr->accept(*this);           // inside expr, this would help get value
                                            // VarDef hold the place for alloca
    }
    else{
        int i = 0;
        int num = node.elementList.size();
        for(auto item: node.elementList){
            item->accept(*this);
            i++;
        }                                   // we will wait to see if there needs more data pass
    }
    return;
}

void IRBuilder::visit(SyntaxTree::FuncDef &node) {}

void IRBuilder::visit(SyntaxTree::FuncFParamList &node) {}

void IRBuilder::visit(SyntaxTree::FuncParam &node) {}

void IRBuilder::visit(SyntaxTree::VarDef &node) {   // we will need to know glob/local here
    bool is_array = false;
    auto zero_initializer = ConstantZero::get(Int32Type, module.get());
    if(is_global)
    {

    }
    if (node.is_constant)
    {
        static ConstantInt *get(,module.get())
    }
        //std::cout << "const ";
    //std::cout << type2str[node.btype] << " " << node.name;
    for (auto length : node.array_length) {
        //std::cout << "[";
        length->accept(*this);
        //std::cout << "]";
        is_array = true;
    }
    if (node.is_inited) {
        //int tmp = indent;
        //indent = 0;
        std::cout << " = ";
        node.initializers->accept(*this);
        //indent = tmp;
    }
    //std::cout << ";" << std::endl;
}

void IRBuilder::visit(SyntaxTree::LVal &node) {}

void IRBuilder::visit(SyntaxTree::AssignStmt &node) {}

void IRBuilder::visit(SyntaxTree::Literal &node) {}

void IRBuilder::visit(SyntaxTree::ReturnStmt &node) {}

void IRBuilder::visit(SyntaxTree::BlockStmt &node) {}

void IRBuilder::visit(SyntaxTree::EmptyStmt &node) {}

void IRBuilder::visit(SyntaxTree::ExprStmt &node) {}

void IRBuilder::visit(SyntaxTree::UnaryCondExpr &node) {}

void IRBuilder::visit(SyntaxTree::BinaryCondExpr &node) {}

void IRBuilder::visit(SyntaxTree::BinaryExpr &node) {}

void IRBuilder::visit(SyntaxTree::UnaryExpr &node) {}

void IRBuilder::visit(SyntaxTree::FuncCallStmt &node) {}

void IRBuilder::visit(SyntaxTree::IfStmt &node) {}

void IRBuilder::visit(SyntaxTree::WhileStmt &node) {}

void IRBuilder::visit(SyntaxTree::BreakStmt &node) {}

void IRBuilder::visit(SyntaxTree::ContinueStmt &node) {}
