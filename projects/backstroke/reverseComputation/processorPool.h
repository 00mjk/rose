#include <rose.h>
#include <boost/function.hpp>
#include <boost/foreach.hpp>

#include "statementHandler.h"

#include "types.h"
#include "singleton.h"
#include "normalizations/expNormalization.h"

#define foreach BOOST_FOREACH

using namespace boost;

typedef boost::function<ExpPair(SgExpression*)> ExpHandler;
typedef boost::function<std::vector<StmtPair>(SgStatement*)> StmtHandler;
typedef std::pair<SgFunctionDeclaration*, SgFunctionDeclaration*> FuncDeclPair;

typedef singleton_default<std::vector<ExpHandler> > ExpHandlersPool;
typedef singleton_default<std::vector<StmtHandler> > StmtHandlersPool;

inline std::vector<ExpPair> processExpression(SgExpression* exp)
{
    std::vector<ExpPair> outputs;
    foreach (ExpHandler handler, ExpHandlersPool::instance())
    {
        ExpPair result = handler(exp);
        if (result != NULL_EXP_PAIR)
            outputs.push_back(handler(exp));
    }
    return outputs;
}

inline std::vector<StmtPair> processStatement(SgStatement* stmt)
{
    std::vector<StmtPair> outputs;
    foreach (StmtHandler handler, StmtHandlersPool::instance())
    {
        std::vector<StmtPair> stmt_pairs = handler(stmt);
        outputs.insert(outputs.end(), stmt_pairs.begin(), stmt_pairs.end());
    }
    return outputs;
}

inline void addExpressionHandler(ExpHandler exp_handler)
{
    ExpHandlersPool::instance().push_back(exp_handler);
}

inline void addStatementHandler(StmtHandler stmt_handler)
{
    StmtHandlersPool::instance().push_back(stmt_handler);
}

inline SgExpression* pushVal(SgExpression* exp)
{
    return SageInterface::copyExpression(exp);
}

inline SgExpression* popVal()
{
    return SageBuilder::buildIntVal(0);
}

class ProcessorPool
{
    //std::vector<ExpHandler> exp_handlers_;
    //std::vector<StmtHandler> stmt_handlers_;


    public:

#if 1
    std::vector<FuncDeclPair>
    processEvent(SgFunctionDeclaration* func_decl)
    {
        // First of all, normalize this event function.
        normalizeEvent(func_decl);

        std::vector<FuncDeclPair> output;

        std::vector<StmtPair> func_decls = processFunctionDeclaration(func_decl);
        foreach (StmtPair stmt_pair, func_decls)
        {
            SgFunctionDeclaration* fwd_decl = isSgFunctionDeclaration(stmt_pair.first);
            SgFunctionDeclaration* rvs_decl = isSgFunctionDeclaration(stmt_pair.second);

            if (fwd_decl && rvs_decl)
                output.push_back(std::make_pair(fwd_decl, rvs_decl));
        }

        return output;
    }
#endif
};

class StatementProcessorPool
{
};
