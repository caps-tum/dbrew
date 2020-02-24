/* Expression tree for analyzed information,
 * helps also with various optimizations.
 *
 * Examples for analying memory accesses:
 * - with "double a[50][50]":
 *   for "a[i+1][j+1]" we see "a + 400*i + 8*j + 408"
 *   with i par1, j par2:
 *    Ref(a, Sum(Scaled(400, Par(1)), Sum(Scaled(8, Par(2)), Const(408)))
 *
 * If element/dimension sizes known, reconstruction is possible
 */

#ifndef EXPR_H
#define EXPR_H

#include <stdint.h>

typedef enum _NodeType {
    NT_Invalid, NT_Const, NT_Sum, NT_Scaled, NT_Par, NT_Ref
} NodeType;

typedef struct _ExprNode ExprNode;
typedef struct _ExprPool ExprPool;

#define EN_NAMELEN 8
struct _ExprNode {
    ExprPool* p;
    NodeType type;

    int ival;      // Const, Scaled: scaling factor, FuncPar: par no
    uint64_t ptr;  // Ref: base pointer
    int left;      // Sum: Op1, Ref: index
    int right;     // Sum: Op2
    char name[EN_NAMELEN]; // Par: parameter name, Ref: array name
};

struct _ExprPool {
    int size;
    int used;
    ExprNode n[1];
};

ExprPool* expr_allocPool(int s);
void expr_freePool(ExprPool* p);
ExprNode* expr_newNode(ExprPool* p, NodeType t);
int expr_nodeIndex(ExprPool* p, ExprNode* n);

ExprNode* expr_newConst(ExprPool* p, int val);
ExprNode* expr_newPar(ExprPool* p, int no, char* n);
ExprNode* expr_newScaled(ExprPool* p, int factor, ExprNode* e);
ExprNode* expr_newRef(ExprPool* p, uint64_t ptr, char* n, ExprNode* idx);
ExprNode* expr_newSum(ExprPool* p, ExprNode* left, ExprNode* right);

char* expr_toString(ExprNode* e);


#endif // EXPR_H

