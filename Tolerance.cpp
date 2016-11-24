#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/CFG.h"

using namespace llvm;

namespace {
  typedef std::map<Value*, Value*> VectorizeMapTable;
/**===================VectorizeMap========================**/
  class VectorizeMap {
    VectorizeMapTable vmap;

    public:
    VectorizeMap(): vmap() {}
    bool IsVectorize(Value *sca) {
	return vmap.find(sca)!=vmap.end();
    }
    void AddPair(Value* sca, Value* vec) {
	VectorizeMapTable::iterator iter=vmap.find(sca);
        if(iter == vmap.end())//do not find vectorize
	  vmap.insert(std::make_pair(sca, vec));
        else
          errs()<<*sca <<"has been vectorized.\n";
    }
    Value* GetVector(Value* sca) {
        VectorizeMapTable::iterator iter=vmap.find(sca);
        if(iter != vmap.end())//find vectorize
           return iter->second;
        else
           return NULL;
    }
    VectorizeMapTable::iterator GetBegin() {
        return vmap.begin();
    }
    VectorizeMapTable::iterator GetEnd() {
        return vmap.end();
    }
  };
  void PrintMap(VectorizeMap *map)
  {
        errs() <<"~Print Map:\n";
        VectorizeMapTable::iterator iter;
        for(iter = map->GetBegin();iter!=map->GetEnd();iter++){
            Value* v_sca=iter->first;
            Value* v_vec=iter->second;
            errs() << *v_sca << ": " << *v_vec << "\n";
        }
  }
  struct TolerancePass : public FunctionPass {
    static char ID;
    TolerancePass() : FunctionPass(ID) {}
    
    virtual bool runOnFunction(Function &F) {
      errs() << "function name: " << F.getName() << "\n";

      //errs() << "Function body:\n";
      //F.dump();
      VectorizeMap vec_map;
      for (auto &B : F) {
        //errs() << "Basic block:\n";
        //B.dump();
       
        for (auto &I : B) {
        //BasicBlock* bb=I.getParent();
        //Find allocation instruction
        if (auto *op = dyn_cast<AllocaInst>(&I)) {
            //errs()<< "Find AllocaInst!\n";
            
            IRBuilder<> builder(op);
            Type* scalar_t= op->getAllocatedType();
            //errs()<<*scalar_t<<"\n";
            //support inst type?
            if(scalar_t->isIntegerTy()){
                errs()<<"Find integer:"<<*scalar_t<<"\n";
	        auto allocaVec = builder.CreateAlloca(VectorType::get(op->getType(), 4));
                allocaVec->setAlignment(16);
                errs()<<"address sca:"<<*op<<",vec:"<<*allocaVec<<"\n";
                vec_map.AddPair(op,allocaVec);
                //get vector demo
                auto *vec = vec_map.GetVector(op);
		//errs()<<"!get vector:"<<op->getOperand(0)<<"\n";
                errs()<<"get vector:"<<*vec<<"\n";
            }  
            if(scalar_t->isFloatTy()){
                errs()<<"Find float:"<<"*scalar_t"<<"\n";
                /*auto allocaVec = builder.CreateAlloca(VectorType::get(op->getType(), 4));
                allocaVec->setAlignment(4);*/
            }
            if(scalar_t->isVectorTy())
                errs()<<"Find vector:"<<*scalar_t<<"\n";
        }
        //Find store instruction 
        else if (StoreInst *op = dyn_cast<StoreInst>(&I)) {
            IRBuilder<> builder(op);
            Value* store_v=op->getValueOperand();//value
	    errs()<<"~Find store_v:"<<*store_v<<"\n";
            Value* store_p=op->getPointerOperand();//pointer
	    errs()<<"~Find store_p:"<<*store_p<<"\n";
             //Value* load_val=builder.CreateLoad(store_p);
            Type* load_ty= store_p->getType();
            errs()<<"~Load type:"<<*load_ty<<"\n";
            
           
            //create store into original vector register
            //auto *map_vec = vec_map.GetVector(store_p);
            //Instruction to Value type
            Value* gop=op->getOperand(0);
            Value* gop1=op->getOperand(1);
            errs()<<"~~get store:"<<*gop<<"\n";
	    errs()<<"~~get store1:"<<*gop1<<"\n";
            //errs()<<"~~get insertelement:"<<*val<<"\n";
        }
        //Find load instruction & create vector before loadinst
        else if (auto *op = dyn_cast<LoadInst>(&I)) {
            IRBuilder<> builder(op);
            Value* load_p=op->getPointerOperand();
            Type* load_ty= load_p->getType();
            errs()<<"~Load type:"<<*load_ty<<"\n";
            errs()<<"*~Find Load_p:"<<*load_p<<"\n";
            errs()<<"@createInsertElement:\n";
            Value* val = UndefValue::get(VectorType::get(load_ty, 4));
            for (unsigned i = 0; i < 4; i++)//for 4 copies
            {
                auto load_val=builder.CreateLoad(load_p);
                load_val->setAlignment(4);
                Value* get_op=load_val->getOperand(0);
		val = builder.CreateInsertElement(val, get_op, builder.getInt32(i));
            }
            //create load & create insertelement instruction?
        }
        //Find operator to neon duplication
        else if (auto *op = dyn_cast<BinaryOperator>(&I)) {
            errs()<<"~Find BinaryOperator\n";
            // Insert at the point where the instruction `op` appears.
            //IRBuilder<> builder(op);
            // Make a multiply with the same operands as `op`.
            //Value* lhs = op->getOperand(0);
            //Value* rhs = op->getOperand(1);
            //Value* mul = builder.CreateMul(lhs, rhs);
            // Everywhere the old instruction was used as an operand, use our
     	    // new multiply instruction instead.
            /*for (auto& U : op->uses()) {
                User* user = U.getUser();  // A User is anything with operands.
                user->setOperand(U.getOperandNo(), mul);
            }*/
          }
          I.dump();
 
        }
      }
      PrintMap(&vec_map);
      return true;
    }
  };
}
/**===================end of VectorizeMap========================/**/
char TolerancePass::ID = 0;
static RegisterPass<TolerancePass> X("tolerance", "Tolerance Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

