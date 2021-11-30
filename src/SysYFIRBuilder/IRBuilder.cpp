#include "IRBuilder.h"

#define CONST_INT(num) ConstantInt::get(num, module.get())
#define CONST_FLOAT(num) ConstantFloat::get(num, module.get())

// You can define global variables and functions here
int is_global = 0; // at first it is global
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
std::map<SyntaxTree::Type,Type *> TypeMap;

void IRBuilder::visit(SyntaxTree::Assembly &node) {
    VOID_T = Type::get_void_type(module.get());
    INT1_T = Type::get_int1_type(module.get());
    INT32_T = Type::get_int32_type(module.get());
    FLOAT_T = Type::get_float_type(module.get());
    INT32PTR_T = Type::get_int32_ptr_type(module.get());
    FLOATPTR_T = Type::get_float_ptr_type(module.get());

    TypeMap[SyntaxTree::Type::VOID]=VOID_T;
    TypeMap[SyntaxTree::Type::BOOL]=INT1_T;
    TypeMap[SyntaxTree::Type::INT]=INT32_T;
    TypeMap[SyntaxTree::Type::FLOAT]=FLOAT_T;

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

void IRBuilder::visit(SyntaxTree::FuncDef &node) {
    std::vector<Type *> params;
    is_global++;
    for(auto param:node.param_list->params){
        params.push_back(TypeMap[param->param_type]);
    }
    auto FuncType = FunctionType::get(TypeMap[node.ret_type], params);
    auto Func = Function::create(FuncType, node.name, builder->get_module());
    scope.push(node.name,Func);
    auto bb = BasicBlock::create(builder->get_module(), "entry", Func);
    builder->set_insert_point(bb);
    scope.enter();
    node.param_list->accept(*this);
    if(node.name=="main"){
        builder->create_call(Func, std::vector<Value *>{});
    }
    for (auto statement : node.body->body) {
        statement->accept(*this);
        if(dynamic_cast<SyntaxTree::ReturnStmt*>(statement.get())){
            break;
        }
    }
    scope.exit();
    is_global--;
}

void IRBuilder::visit(SyntaxTree::FuncFParamList &node) {
    auto Argument = builder->get_module()->get_functions().back()->arg_begin();//当前的函数应该是函数表中最后一个函数
    for(auto param:node.params){
        param->accept(*this);                                               //访问参数
        auto paramAlloc=builder->create_alloca(TypeMap[param->param_type]);     //分配空间
        builder->create_store(*Argument,paramAlloc);                            //存参数的值
        scope.push(param->name,paramAlloc);                              //加入符号表
        Argument++;                                                             //下一个参数值
    }
}

void IRBuilder::visit(SyntaxTree::FuncParam &node) {
    for(auto Exp:node.array_index){                                             //遍历每个Expr
        if (Exp != nullptr){
            Exp->accept(*this);
        }
    }
}

void IRBuilder::visit(SyntaxTree::VarDef &node) {   // we will need to know glob/local here
    bool is_array = false;
    if(node.btype == SyntaxTree::Type::INT)
    {
        auto zero_initializer = ConstantZero::get(INT32_T, module.get());
        if(!is_global)
        {
            for (auto length : node.array_length) {
                length->accept(*this);
                is_array = true;
            }   // this should not exist
            if (node.is_inited) 
            {
                node.initializers->accept(*this);   // get me the initial value // how?
                auto global_init = GlobalVariable::create(node.name, module.get(), INT32_T, true, zero_initializer);
            }
            else
            {
                if (node.is_constant)
                    auto global_init = GlobalVariable::create(node.name, module.get(), INT32_T, true, zero_initializer);
                else
                    auto global_init = GlobalVariable::create(node.name, module.get(), INT32_T, false, zero_initializer);
            }
        }
        else
        {
            for (auto length : node.array_length) {
                length->accept(*this);
                is_array = true;
            }   // this should exist but wait a minute
            if (node.is_inited) 
            {
                node.initializers->accept(*this);   // get me the initial value // how?
                auto Alloca = builder->create_alloca(INT32_T); // should we do more about this alloca?
                auto Load = builder->create_load(CONST_INT(0)); // where to get data?
            }
            else
            {
                if (node.is_constant)
                    auto Alloca = builder->create_alloca(INT32_T);
                else
                    auto Alloca = builder->create_alloca(INT32_T);
            }
        }
    }
    else        // FLOAT
    {
        auto zero_initializer = ConstantZero::get(FLOAT_T, module.get());
        if(!is_global)
        {
            for (auto length : node.array_length) {
                length->accept(*this);
                is_array = true;
            }   // this should not exist
            if (node.is_inited) 
            {
                node.initializers->accept(*this);   // get me the initial value // how?
                auto global_init = GlobalVariable::create(node.name, module.get(), FLOAT_T, true, zero_initializer);
            }
            else
            {
                if (node.is_constant)
                    auto global_init = GlobalVariable::create(node.name, module.get(), FLOAT_T, true, zero_initializer);
                else
                    auto global_init = GlobalVariable::create(node.name, module.get(), FLOAT_T, false, zero_initializer);
            }
        }
        else
        {
            for (auto length : node.array_length) {
                length->accept(*this);
                is_array = true;
            }   // this should exist but wait a minute
            if (node.is_inited) 
            {
                node.initializers->accept(*this);   // get me the initial value // how?
                auto Alloca = builder->create_alloca(FLOAT_T); // should we do more about this alloca?
                auto Load = builder->create_load(CONST_FLOAT(0)); // where to get data?
            }
            else
            {
                if (node.is_constant)
                    auto Alloca = builder->create_alloca(FLOAT_T);
                else
                    auto Alloca = builder->create_alloca(FLOAT_T);
            }
        }
    }
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
