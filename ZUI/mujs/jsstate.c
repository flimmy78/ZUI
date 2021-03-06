#include "jsi.h"
#include "jsparse.h"
#include "jscompile.h"
#include "jsvalue.h"
#include "jsrun.h"
#include "jsbuiltin.h"

#include <assert.h>
#include "../core/memory.h"
#include <ZUI.h>
static void js_defaultpanic(js_State *J)
{
    fwprintf(stderr, L"uncaught exception: %ls\n", js_tostring(J, -1));
    /* return to javascript to abort */
}

int js_ploadstring(js_State *J, const wchar_t *filename, const wchar_t *source)
{
    if (js_try(J))
        return 1;
    js_loadstring(J, filename, source);
    js_endtry(J);
    return 0;
}

int js_ploadfile(js_State *J, const wchar_t *filename)
{
    if (js_try(J))
        return 1;
    js_loadfile(J, filename);
    js_endtry(J);
    return 0;
}

static void js_loadstringx(js_State *J, const wchar_t *filename, const wchar_t *source, int iseval)
{
    js_Ast *P;
    js_Function *F;

    if (js_try(J)) {
        jsP_freeparse(J);
        js_throw(J);
        fprintf(stderr, "%ls\n", js_tostring(J, -1));
    }

    P = jsP_parse(J, filename, source);
    F = jsC_compile(J, P);
    jsP_freeparse(J);
    js_newscript(J, F, iseval ? (J->strict ? J->E : NULL) : J->GE);

    js_endtry(J);
}

void js_loadeval(js_State *J, const wchar_t *filename, const wchar_t *source)
{
    js_loadstringx(J, filename, source, 1);
}

void js_loadstring(js_State *J, const wchar_t *filename, const wchar_t *source)
{
    js_loadstringx(J, filename, source, 0);
}

void js_loadfile(js_State *J, const wchar_t *filename)
{
    FILE *f;
    wchar_t *s;
    int n, t;

    f = _wfopen(filename, L"rb");
    if (!f) {
        js_error(J, L"cannot open file: '%ls'", filename);
    }

    if (fseek(f, 0, SEEK_END) < 0) {
        fclose(f);
        js_error(J, L"cannot seek in file: '%ls'", filename);
    }

    n = ftell(f);
    if (n < 0) {
        fclose(f);
        js_error(J, L"cannot tell in file: '%ls'", filename);
    }

    if (fseek(f, 0, SEEK_SET) < 0) {
        fclose(f);
        js_error(J, L"cannot seek in file: '%ls'", filename);
    }

    s = ZuiMalloc(n + 1); /* add space for string terminator */
    if (!s) {
        fclose(f);
        js_error(J, L"cannot allocate storage for file contents: '%ls'", filename);
    }

    t = fread(s, 1, (size_t)n, f);
    if (t != n) {
        ZuiFree(s);
        fclose(f);
        js_error(J, L"cannot read data from file: '%ls'", filename);
    }

    s[n] = 0; /* zero-terminate string containing file data */

    if (js_try(J)) {
        ZuiFree(s);
        fclose(f);
        js_throw(J);
    }

    js_loadstring(J, filename, s);

    ZuiFree(s);
    fclose(f);
    js_endtry(J);
}

int js_dostring(js_State *J, const wchar_t *source)
{
    if (js_try(J)) {
        fprintf(stderr, "%ls\n", js_tostring(J, -1));
        js_pop(J, 1);
        return 1;
    }
    js_loadstring(J, L"[string]", source);
    js_pushglobal(J);
    js_call(J, 0);
    js_pop(J, 1);
    js_endtry(J);
    return 0;
}

int js_dofile(js_State *J, const wchar_t *filename)
{
    if (js_try(J)) {
        fprintf(stderr, "%ls\n", js_tostring(J, -1));
        js_pop(J, 1);
        return 1;
    }
    js_loadfile(J, filename);
    js_pushglobal(J);
    js_call(J, 0);
    js_pop(J, 1);
    js_endtry(J);
    return 0;
}

js_Panic js_atpanic(js_State *J, js_Panic panic)
{
    js_Panic old = J->panic;
    J->panic = panic;
    return old;
}

void js_setcontext(js_State *J, void *uctx)
{
    J->uctx = uctx;
}

void *js_getcontext(js_State *J)
{
    return J->uctx;
}

js_State *js_newstate(int flags)
{
    js_State *J;

    assert(sizeof(js_Value) == 32);
    assert(soffsetof(js_Value, type) == 30);

    J = ZuiMalloc(sizeof *J);
    if (!J)
        return NULL;
    memset(J, 0, sizeof(*J));

    if (flags & JS_STRICT)
        J->strict = 1;

    J->trace[0].name = L"-top-";
    J->trace[0].file = L"native";
    J->trace[0].line = 0;

    J->panic = js_defaultpanic;

    J->stack = ZuiMalloc(JS_STACKSIZE * sizeof *J->stack);

    J->gcmark = 1;
    J->nextref = 0;

    J->R = jsV_newobject(J, JS_COBJECT, NULL);
    J->G = jsV_newobject(J, JS_COBJECT, NULL);
    J->E = jsR_newenvironment(J, J->G, NULL);
    J->GE = J->E;

    jsB_init(J);

    return J;
}
