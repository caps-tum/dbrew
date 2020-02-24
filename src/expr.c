/**
 * This file is part of DBrew, the dynamic binary rewriting library.
 *
 * (c) 2015-2016, Josef Weidendorfer <josef.weidendorfer@gmx.de>
 *
 * DBrew is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License (LGPL)
 * as published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * DBrew is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with DBrew.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "expr.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

ExprPool* expr_allocPool(int s)
{
    ExprPool* p;

    p = (ExprPool*) malloc(sizeof(ExprPool) + s * sizeof(ExprNode));
    p->size = s;
    p->used = 0;
    return p;
}

void expr_freePool(ExprPool* p)
{
    free(p);
}

ExprNode* expr_newNode(ExprPool* p, NodeType t)
{
    ExprNode* e;

    assert(p && (p->used < p->size));
    e = p->n + p->used;
    e->p = p;
    e->type = t;

    p->used++;
    return e;
}

int expr_nodeIndex(ExprPool* p, ExprNode* n)
{
    assert(p && n && n->p == p);
    return (int)(n - p->n);
}

ExprNode* expr_newConst(ExprPool* p, int val)
{
    ExprNode* e = expr_newNode(p, NT_Const);
    e->ival = val;

    return e;
}

ExprNode* expr_newPar(ExprPool* p, int no, char *n)
{
    ExprNode* e = expr_newNode(p, NT_Par);
    e->ival = no;
    if (n) {
        int i = 0;
        while(n[i] && (i < EN_NAMELEN-1)) {
            e->name[i] = n[i];
            i++;
        }
        e->name[i] = 0;
    }
    else
        e->name[0] = 0;

    return e;
}

ExprNode* expr_newScaled(ExprPool* p, int factor, ExprNode* exp)
{
    ExprNode* e = expr_newNode(p, NT_Scaled);
    e->ival = factor;
    e->left = expr_nodeIndex(p, exp);

    return e;
}

ExprNode* expr_newRef(ExprPool* p, uint64_t ptr, char* n, ExprNode* idx)
{
    ExprNode* e = expr_newNode(p, NT_Ref);
    e->ptr = ptr;
    e->left = expr_nodeIndex(p, idx);
    if (n) {
        int i = 0;
        while(n[i] && (i < EN_NAMELEN-1)) {
            e->name[i] = n[i];
            i++;
        }
        e->name[i] = 0;
    }
    else
        e->name[0] = 0;

    return e;
}

ExprNode* expr_newSum(ExprPool* p, ExprNode* left, ExprNode* right)
{
    ExprNode* e = expr_newNode(p, NT_Sum);
    e->left = expr_nodeIndex(p, left);
    e->right = expr_nodeIndex(p, right);

    return e;
}

static
int appendExpr(char* b, ExprNode* e)
{
    int off = 0;

    switch(e->type) {
    case NT_Const:
        return sprintf(b, "%d", e->ival);

    case NT_Par:
        if (e->name[0])
            return sprintf(b, "%s", e->name);
        return sprintf(b, "par%d", e->ival);

    case NT_Ref:
        if (e->name[0])
            off = sprintf(b, "%s", e->name);
        else
            off = sprintf(b, "%lx", e->ptr);
        b[off] = '[';
        off += appendExpr(b+off, e->p->n + e->left);
        off += sprintf(b, "]");
        return off;

    case NT_Scaled:
        off =  sprintf(b, "%d * ", e->ival);
        off += appendExpr(b+off, e->p->n + e->left);
        return off;

    case NT_Sum:
        off =  appendExpr(b, e->p->n + e->left);
        off += sprintf(b+off, " + ");
        off += appendExpr(b+off, e->p->n + e->right);
        return off;

    default: assert(0);
    }
    return 0;
}

char *expr_toString(ExprNode *e)
{
    static char buf[200];
    int off;
    off = appendExpr(buf, e);
    assert(off < 200);
    return buf;
}
