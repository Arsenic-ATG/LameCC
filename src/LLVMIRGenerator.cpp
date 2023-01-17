#include "lcc.hpp"

#define LLVMIRGEN_RET_TRUE(val) \
    {                           \
        _retVal = val;          \
        return true;            \
    }
#define LLVMIRGEN_RET_FALSE() \
    {                         \
        _retVal = nullptr;    \
        return false;         \
    }

namespace lcc
{
    std::unique_ptr<LLVMIRGenerator> LLVMIRGenerator::_inst = nullptr;

    LLVMIRGenerator::LLVMIRGenerator()
    {
        _builder = std::make_unique<llvm::IRBuilder<>>(_context);
        _module = std::make_unique<llvm::Module>("LCC_LLVMIRGenerator", _context);

        changeTable(mkTable());
    }

    void LLVMIRGenerator::dumpCode(const std::string outPath) const
    {
        // TODO dumper
        std::error_code err;
        auto os = std::make_unique<llvm::raw_fd_ostream>(outPath, err);
        _module->print(*os, nullptr, false, false);
        return;
    }

    void LLVMIRGenerator::printCode() const
    {
        // TODO debug print
        _module->print(llvm::outs(), nullptr);
    }

    std::shared_ptr<LLVMIRGenerator::SymbolTable> LLVMIRGenerator::mkTable(std::shared_ptr<SymbolTable> previous)
    {
        auto tbl = std::make_shared<SymbolTable>(previous);
        _tables.push_back(tbl);
        return tbl;
    }

    void LLVMIRGenerator::changeTable(std::shared_ptr<SymbolTable> table)
    {
        _currentSymbolTable = table;
    }

    llvm::Value *LLVMIRGenerator::lookupCurrentTbl(std::string name)
    {
        if (_currentSymbolTable->symbls.find(name) == _currentSymbolTable->symbls.end())
            return nullptr;

        return _currentSymbolTable->symbls[name];
    }

    llvm::Value *LLVMIRGenerator::lookup(std::string name)
    {
        auto currentTbl = _currentSymbolTable;
        llvm::Value *pVal = nullptr;
        for (_currentSymbolTable; _currentSymbolTable != nullptr; changeTable(_currentSymbolTable->previous))
        {
            pVal = lookupCurrentTbl(name);
            if (pVal != nullptr)
                break;
        }
        changeTable(currentTbl);

        return pVal;
    }

    bool LLVMIRGenerator::enter(std::string name, llvm::AllocaInst *alloca)
    {
        if (lookupCurrentTbl(name) != nullptr)
            return false;

        _currentSymbolTable->symbls.insert(std::make_pair(name, alloca));
        return true;
    }

    llvm::AllocaInst *LLVMIRGenerator::createEntryBlockAlloca(llvm::Function *function, const std::string &name, const std::string type)
    {
        llvm::IRBuilder<> builder(&function->getEntryBlock(), function->getEntryBlock().begin());

        if (type == "int")
            return builder.CreateAlloca(llvm::Type::getInt32Ty(_context), 0, name.c_str());
        else if (type == "float")
            return builder.CreateAlloca(llvm::Type::getFloatTy(_context), 0, name.c_str());
        else
            return nullptr;
    }

    bool LLVMIRGenerator::gen(AST::TranslationUnitDecl *translationUnitDecl)
    {
        for (auto &decl : translationUnitDecl->_decls)
            if (!decl->gen(this))
                LLVMIRGEN_RET_FALSE();

        LLVMIRGEN_RET_TRUE(nullptr);
    }

    bool LLVMIRGenerator::gen(AST::FunctionDecl *functionDecl)
    {
        std::vector<llvm::Type *> params;
        for (auto &param : functionDecl->_params)
        {
            if (param->type() == "int")
                params.push_back(llvm::Type::getInt32Ty(_context));
            else if (param->type() == "char")
                params.push_back(llvm::Type::getFloatTy(_context));
            else
                LLVMIRGEN_RET_FALSE();
        }

        llvm::FunctionType *ft = nullptr;

        if (functionDecl->_type == "void")
            ft = llvm::FunctionType::get(llvm::Type::getVoidTy(_context), params, false);
        else if (functionDecl->_type == "int")
            ft = llvm::FunctionType::get(llvm::Type::getInt32Ty(_context), params, false);
        else if (functionDecl->_type == "float")
            ft = llvm::FunctionType::get(llvm::Type::getFloatTy(_context), params, false);
        else
        {
            FATAL_ERROR("Unsupported return type for function " << functionDecl->name());
            LLVMIRGEN_RET_FALSE();
        }

        auto func = _module->getFunction(functionDecl->name());

        if (func != nullptr)
        {
            if (!func->empty() || (functionDecl->_body != nullptr))
            {
                FATAL_ERROR("Redefinition function " << functionDecl->name());
                LLVMIRGEN_RET_FALSE();
            }
            else if (func->arg_size() != functionDecl->_params.size())
            {
                FATAL_ERROR("Function " << functionDecl->name() << "definition deosn't match with declaration");
                LLVMIRGEN_RET_FALSE();
            }
        }
        else
            func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, functionDecl->name(), _module.get());

        unsigned int idx = 0;
        for (auto &arg : func->args())
        {
            arg.setName(functionDecl->_params[idx++]->name());
        }

        if (functionDecl->_body == nullptr)
            LLVMIRGEN_RET_TRUE(func);

        llvm::BasicBlock *bb = llvm::BasicBlock::Create(_context, "entry", func);
        llvm::BasicBlock *retvalbb = llvm::BasicBlock::Create(_context, "return", func);

        _builder->SetInsertPoint(retvalbb);

        if (functionDecl->_type == "void")
            _curFuncRetAlloca = nullptr;
        else if (
            functionDecl->_type == "int" ||
            functionDecl->_type == "float"
        )
        {
            _curFuncRetAlloca = createEntryBlockAlloca(func, "retVal", functionDecl->_type);
        }
        else
        {
            FATAL_ERROR("Unsupported return value in function " << functionDecl->name());
            LLVMIRGEN_RET_FALSE();
        }

        if(_curFuncRetAlloca)
        {
            // Probably need a load here
            //auto retVal = _builder->CreateLoad(_curFuncRetAlloca);
            _builder->CreateRet(_curFuncRetAlloca);
        }
        else _builder->CreateRetVoid(); // emit ret void

        _builder->SetInsertPoint(bb);

        auto previousTable = _currentSymbolTable;
        changeTable(mkTable(previousTable)); // create a new scope for function params

        // Alloc space for function params
        for (auto &arg : func->args())
        {
            llvm::AllocaInst *alloca = nullptr;
            if (arg.getType()->isIntegerTy())
                alloca = createEntryBlockAlloca(func, arg.getName().str(), "int");
            else if (arg.getType()->isFloatTy())
                alloca = createEntryBlockAlloca(func, arg.getName().str(), "float");
            else
            {
                FATAL_ERROR("Unsupported param type in function " << func->getName().str());
                arg.getType()->print(llvm::outs());
                LLVMIRGEN_RET_FALSE();
            }

            _builder->CreateStore(&arg, alloca);
            enter(arg.getName().str(), alloca);
        }

        functionDecl->_body->gen(this);

        _builder->CreateBr(retvalbb); // unconditional jump to return bb after function body

        changeTable(previousTable);
        LLVMIRGEN_RET_TRUE(nullptr);
    }

    bool LLVMIRGenerator::gen(AST::VarDecl *varDecl)
    {
        if (lookupCurrentTbl(varDecl->name()) != nullptr)
        {
            FATAL_ERROR("Redefinition " << varDecl->type() << " " << varDecl->name());
            LLVMIRGEN_RET_FALSE();
        }

        varDecl->place = varDecl->name();

        llvm::Value *initVal = nullptr;
        if (varDecl->_isInitialized)
        {
            if (!varDecl->_value->gen(this))
                LLVMIRGEN_RET_FALSE();

            initVal = _retVal;
        }

        auto ib = _builder->GetInsertBlock();
        if (ib)
        {
            auto function = ib->getParent();
            auto alloca = createEntryBlockAlloca(function, varDecl->name(), varDecl->type());
            enter(varDecl->name(), alloca);
            if (initVal != nullptr)
            {
                auto store = _builder->CreateStore(initVal, alloca);
                LLVMIRGEN_RET_TRUE(store);
            }
        }
        else
        {
            if (_module->getGlobalVariable(varDecl->name()))
            {
                FATAL_ERROR("Redeclaration global variable" << varDecl->type() << " " << varDecl->name());
                LLVMIRGEN_RET_FALSE();
            }

            if (varDecl->type() == "int")
            {
                llvm::GlobalVariable *gVar = new llvm::GlobalVariable(
                    *_module, llvm::Type::getInt32Ty(_context), false,
                    llvm::GlobalValue::ExternalLinkage,
                    _builder->getInt32(0),
                    varDecl->name());

                LLVMIRGEN_RET_TRUE(gVar);
            }
            // else if(varDecl->type() == "float")
            // {
            //     llvm::GlobalVariable* gVar = new llvm::GlobalVariable(
            //         *_module, llvm::Type::getFloatTy(_context), false,
            //         llvm::GlobalValue::ExternalLinkage,
            //         llvm::Constant::ConstantFPVal(0.f),
            //         varDecl->name());

            //     LLVMIRGEN_RET_TRUE(gVar);
            // }
            else
            {
                LLVMIRGEN_RET_FALSE();
            }
        }

        return true;
    }

    bool LLVMIRGenerator::gen(AST::IntegerLiteral *integerLiteral)
    {
        auto val = llvm::ConstantInt::get(_context, llvm::APInt(32, integerLiteral->value(), true));
        LLVMIRGEN_RET_TRUE(val);
    }

    bool LLVMIRGenerator::gen(AST::FloatingLiteral *floatingLiteral)
    {
        auto val = llvm::ConstantFP::get(_context, llvm::APFloat(floatingLiteral->value()));
        LLVMIRGEN_RET_TRUE(val);
    }

    bool LLVMIRGenerator::gen(AST::DeclRefExpr *declRefExpr) { return true; }

    bool LLVMIRGenerator::gen(AST::CastExpr *castExpr) { return true; }
    bool LLVMIRGenerator::gen(AST::BinaryOperator *binaryOperator) { return true; }
    bool LLVMIRGenerator::gen(AST::UnaryOperator *unaryOperator) { return true; }
    bool LLVMIRGenerator::gen(AST::ParenExpr *parenExpr) { return true; }

    bool LLVMIRGenerator::gen(AST::CompoundStmt *compoundStmt) 
    { 
        auto previousTable = _currentSymbolTable;
        changeTable(mkTable(previousTable));
        for (auto &stmt : compoundStmt->_body)
        {
            if (!stmt->gen(this))
                LLVMIRGEN_RET_FALSE();
        }

        changeTable(previousTable);
        LLVMIRGEN_RET_TRUE(_retVal);
    }

    bool LLVMIRGenerator::gen(AST::DeclStmt *declStmt) 
    {
        for(auto& decl : declStmt->_decls) 
        {
            if(!decl->gen(this))
                LLVMIRGEN_RET_FALSE();
        }

        LLVMIRGEN_RET_TRUE(_retVal);
    }
    
    bool LLVMIRGenerator::gen(AST::IfStmt *ifStmt) { return true; }
    bool LLVMIRGenerator::gen(AST::ValueStmt *valueStmt) { return true; }
    bool LLVMIRGenerator::gen(AST::ReturnStmt *returnStmt) { return true; }
    bool LLVMIRGenerator::gen(AST::WhileStmt *whileStmt) { return true; }
    bool LLVMIRGenerator::gen(AST::CallExpr *callExpr) { return true; }
}