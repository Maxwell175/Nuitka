//     Copyright 2021, Kay Hayen, mailto:kay.hayen@gmail.com
//
//     Part of "Nuitka", an optimizing Python compiler that is compatible and
//     integrates with CPython, but also works on its own.
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//     Unless required by applicable law or agreed to in writing, software
//     distributed under the License is distributed on an "AS IS" BASIS,
//     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//     See the License for the specific language governing permissions and
//     limitations under the License.
//
// This file is included from another C file, help IDEs to still parse it on
// its own.
#ifdef __IDE_ONLY__
#include "nuitka/prelude.h"
#endif

PyObject *callPythonFunction(PyObject *func, PyObject **args, int count) {
    PyCodeObject *co = (PyCodeObject *)PyFunction_GET_CODE(func);
    PyObject *globals = PyFunction_GET_GLOBALS(func);
    PyObject *argdefs = PyFunction_GET_DEFAULTS(func);

#if PYTHON_VERSION >= 0x300
    PyObject *kwdefs = PyFunction_GET_KW_DEFAULTS(func);

    if (kwdefs == NULL && argdefs == NULL && co->co_argcount == count &&
        co->co_flags == (CO_OPTIMIZED | CO_NEWLOCALS | CO_NOFREE))
#else
    if (argdefs == NULL && co->co_argcount == count && co->co_flags == (CO_OPTIMIZED | CO_NEWLOCALS | CO_NOFREE))
#endif
    {
        PyThreadState *tstate = PyThreadState_GET();
        CHECK_OBJECT(globals);

        PyFrameObject *frame = PyFrame_New(tstate, co, globals, NULL);

        if (unlikely(frame == NULL)) {
            return NULL;
        };

        for (int i = 0; i < count; i++) {
            frame->f_localsplus[i] = args[i];
            Py_INCREF(frame->f_localsplus[i]);
        }

        PyObject *result = PyEval_EvalFrameEx(frame, 0);

        // Frame release protects against recursion as it may lead to variable
        // destruction.
        ++tstate->recursion_depth;
        Py_DECREF(frame);
        --tstate->recursion_depth;

        return result;
    }

    PyObject **defaults = NULL;
    int num_defaults = 0;

    if (argdefs != NULL) {
        defaults = &PyTuple_GET_ITEM(argdefs, 0);
        num_defaults = (int)(Py_SIZE(argdefs));
    }

    PyObject *result = PyEval_EvalCodeEx(
#if PYTHON_VERSION >= 0x300
        (PyObject *)co,
#else
        co, // code object
#endif
        globals,      // globals
        NULL,         // no locals
        args,         // args
        count,        // argcount
        NULL,         // kwds
        0,            // kwcount
        defaults,     // defaults
        num_defaults, // defcount
#if PYTHON_VERSION >= 0x300
        kwdefs,
#endif
        PyFunction_GET_CLOSURE(func));

    return result;
}

static PyObject *_fast_function_noargs(PyObject *func) {
    PyCodeObject *co = (PyCodeObject *)PyFunction_GET_CODE(func);
    PyObject *globals = PyFunction_GET_GLOBALS(func);
    PyObject *argdefs = PyFunction_GET_DEFAULTS(func);

#if PYTHON_VERSION >= 0x300
    PyObject *kwdefs = PyFunction_GET_KW_DEFAULTS(func);

    if (kwdefs == NULL && argdefs == NULL && co->co_argcount == 0 &&
        co->co_flags == (CO_OPTIMIZED | CO_NEWLOCALS | CO_NOFREE))
#else
    if (argdefs == NULL && co->co_argcount == 0 && co->co_flags == (CO_OPTIMIZED | CO_NEWLOCALS | CO_NOFREE))
#endif
    {
        PyThreadState *tstate = PyThreadState_GET();
        CHECK_OBJECT(globals);

        PyFrameObject *frame = PyFrame_New(tstate, co, globals, NULL);

        if (unlikely(frame == NULL)) {
            return NULL;
        };

        PyObject *result = PyEval_EvalFrameEx(frame, 0);

        // Frame release protects against recursion as it may lead to variable
        // destruction.
        ++tstate->recursion_depth;
        Py_DECREF(frame);
        --tstate->recursion_depth;

        return result;
    }

    PyObject **defaults = NULL;
    int num_defaults = 0;

    if (argdefs != NULL) {
        defaults = &PyTuple_GET_ITEM(argdefs, 0);
        num_defaults = (int)(Py_SIZE(argdefs));
    }

    PyObject *result = PyEval_EvalCodeEx(
#if PYTHON_VERSION >= 0x300
        (PyObject *)co,
#else
        co, // code object
#endif
        globals,      // globals
        NULL,         // no locals
        NULL,         // args
        0,            // argcount
        NULL,         // kwds
        0,            // kwcount
        defaults,     // defaults
        num_defaults, // defcount
#if PYTHON_VERSION >= 0x300
        kwdefs,
#endif
        PyFunction_GET_CLOSURE(func));

    return result;
}

PyObject *CALL_METHOD_WITH_POSARGS(PyObject *source, PyObject *attr_name, PyObject *positional_args) {
    CHECK_OBJECT(source);
    CHECK_OBJECT(attr_name);
    CHECK_OBJECT(positional_args);

#if PYTHON_VERSION < 0x300
    if (PyInstance_Check(source)) {
        PyInstanceObject *source_instance = (PyInstanceObject *)source;

        // The special cases have their own variant on the code generation level
        // as we are called with constants only.
        assert(attr_name != const_str_plain___dict__);
        assert(attr_name != const_str_plain___class__);

        // Try the instance dict first.
        PyObject *called_object =
            GET_STRING_DICT_VALUE((PyDictObject *)source_instance->in_dict, (PyStringObject *)attr_name);

        // Note: The "called_object" was found without taking a reference,
        // so we need not release it in this branch.
        if (called_object != NULL) {
            return CALL_FUNCTION_WITH_POSARGS(called_object, positional_args);
        }
        // Then check the class dictionaries.
        called_object = FIND_ATTRIBUTE_IN_CLASS(source_instance->in_class, attr_name);

        // Note: The "called_object" was found without taking a reference,
        // so we need not release it in this branch.
        if (called_object != NULL) {
            descrgetfunc descr_get = Py_TYPE(called_object)->tp_descr_get;

            if (descr_get == Nuitka_Function_Type.tp_descr_get) {
                return Nuitka_CallMethodFunctionPosArgs((struct Nuitka_FunctionObject const *)called_object, source,
                                                        &PyTuple_GET_ITEM(positional_args, 0),
                                                        PyTuple_GET_SIZE(positional_args));
            } else if (descr_get != NULL) {
                PyObject *method = descr_get(called_object, source, (PyObject *)source_instance->in_class);

                if (unlikely(method == NULL)) {
                    return NULL;
                }

                PyObject *result = CALL_FUNCTION_WITH_POSARGS(method, positional_args);
                Py_DECREF(method);
                return result;
            } else {
                return CALL_FUNCTION_WITH_POSARGS(called_object, positional_args);
            }
        } else if (unlikely(source_instance->in_class->cl_getattr == NULL)) {
            PyErr_Format(PyExc_AttributeError, "%s instance has no attribute '%s'",
                         PyString_AS_STRING(source_instance->in_class->cl_name), PyString_AS_STRING(attr_name));

            return NULL;
        } else {
            // Finally allow the "__getattr__" override to provide it or else
            // it's an error.

            PyObject *args[] = {source, attr_name};

            called_object = CALL_FUNCTION_WITH_ARGS2(source_instance->in_class->cl_getattr, args);

            if (unlikely(called_object == NULL)) {
                return NULL;
            }

            PyObject *result = CALL_FUNCTION_WITH_POSARGS(called_object, positional_args);
            Py_DECREF(called_object);
            return result;
        }
    } else
#endif
    {
        PyObject *called_object;

        PyTypeObject *type = Py_TYPE(source);

        if (type->tp_getattro != NULL) {
            called_object = (*type->tp_getattro)(source, attr_name);
        } else if (type->tp_getattr != NULL) {
            called_object = (*type->tp_getattr)(source, (char *)Nuitka_String_AsString_Unchecked(attr_name));
        } else {
            SET_CURRENT_EXCEPTION_TYPE0_FORMAT2(PyExc_AttributeError, "'%s' object has no attribute '%s'",
                                                type->tp_name, Nuitka_String_AsString_Unchecked(attr_name));

            return NULL;
        }

        if (unlikely(called_object == NULL)) {
            return NULL;
        }

        PyObject *result = CALL_FUNCTION_WITH_POSARGS(called_object, positional_args);
        Py_DECREF(called_object);
        return result;
    }
}
