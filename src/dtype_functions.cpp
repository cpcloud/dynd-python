//
// Copyright (C) 2011-12, Dynamic NDArray Developers
// BSD 2-Clause License, see LICENSE.txt
//

#include "dtype_functions.hpp"
#include "ndobject_functions.hpp"
#include "numpy_interop.hpp"
#include "ctypes_interop.hpp"
#include "utility_functions.hpp"

#include <dynd/dtypes/convert_dtype.hpp>
#include <dynd/dtypes/fixedstring_dtype.hpp>
#include <dynd/dtypes/string_dtype.hpp>
#include <dynd/dtypes/pointer_dtype.hpp>
#include <dynd/dtypes/struct_dtype.hpp>
#include <dynd/dtypes/fixedstruct_dtype.hpp>
#include <dynd/dtypes/fixedarray_dtype.hpp>
#include <dynd/dtypes/date_dtype.hpp>
#include <dynd/shape_tools.hpp>

// Python's datetime C API
#include "datetime.h"

using namespace std;
using namespace dynd;
using namespace pydynd;

// Initialize the pydatetime API
namespace {
struct init_pydatetime {
    init_pydatetime() {
        PyDateTime_IMPORT;
    }
};
init_pydatetime pdt;
} // anonymous namespace

PyTypeObject *pydynd::WDType_Type;

void pydynd::init_w_dtype_typeobject(PyObject *type)
{
    pydynd::WDType_Type = (PyTypeObject *)type;
}

PyObject *pydynd::dtype_get_kind(const dynd::dtype& d)
{
    switch (d.get_kind()) {
        case bool_kind:
            return PyString_FromString("bool");
        case int_kind:
            return PyString_FromString("int");
        case uint_kind:
            return PyString_FromString("uint");
        case real_kind:
            return PyString_FromString("real");
        case complex_kind:
            return PyString_FromString("complex");
        case string_kind:
            return PyString_FromString("string");
        case datetime_kind:
            return PyString_FromString("datetime");
        case bytes_kind:
            return PyString_FromString("bytes");
        case void_kind:
            return PyString_FromString("void");
        case struct_kind:
            return PyString_FromString("struct");
        case expression_kind:
            return PyString_FromString("expression");
        case pattern_kind:
            return PyString_FromString("pattern");
        case custom_kind:
            return PyString_FromString("custom");
        default: {
            stringstream ss;
            ss << "dynd dtype " << d << " has unexpected kind value " << (int)d.get_kind();
            throw runtime_error(ss.str());
        }
    }
}

dtype pydynd::deduce_dtype_from_object(PyObject* obj)
{
#if DYND_NUMPY_INTEROP
    if (PyArray_Check(obj)) {
        // Numpy array
        PyArray_Descr *d = PyArray_DESCR((PyArrayObject *)obj);
        return dtype_from_numpy_dtype(d);
    } else if (PyArray_IsScalar(obj, Generic)) {
        // Numpy scalar
        return dtype_of_numpy_scalar(obj);
    }
#endif // DYND_NUMPY_INTEROP
    
    if (PyBool_Check(obj)) {
        // Python bool
        return make_dtype<dynd_bool>();
#if PY_VERSION_HEX < 0x03000000
    } else if (PyInt_Check(obj)) {
        // Python integer
# if SIZEOF_LONG > SIZEOF_INT
        long value = PyInt_AS_LONG(obj);
        // Use a 32-bit int if it fits. This conversion strategy
        // is independent of sizeof(long), and is the same on 32-bit
        // and 64-bit platforms.
        if (value >= INT_MIN && value <= INT_MAX) {
            return make_dtype<int>();
        } else {
            return make_dtype<long>();
        }
# else
        return make_dtype<int>();
# endif
#endif // PY_VERSION_HEX < 0x03000000
    } else if (PyLong_Check(obj)) {
        // Python integer
        PY_LONG_LONG value = PyLong_AsLongLong(obj);
        if (value == -1 && PyErr_Occurred()) {
            throw runtime_error("error converting int value");
        }
        // Use a 32-bit int if it fits. This conversion strategy
        // is independent of sizeof(long), and is the same on 32-bit
        // and 64-bit platforms.
        if (value >= INT_MIN && value <= INT_MAX) {
            return make_dtype<int>();
        } else {
            return make_dtype<PY_LONG_LONG>();
        }
    } else if (PyFloat_Check(obj)) {
        // Python float
        return make_dtype<double>();
    } else if (PyComplex_Check(obj)) {
        // Python complex
        return make_dtype<complex<double> >();
    } else if (PyString_Check(obj)) {
        // Python ascii string
        return make_string_dtype(string_encoding_ascii);
    } else if (PyUnicode_Check(obj)) {
        // Python unicode string
#if Py_UNICODE_SIZE == 2
        return make_string_dtype(string_encoding_ucs_2);
#else
        return make_string_dtype(string_encoding_utf_32);
#endif
    } else if (PyDate_Check(obj)) {
        return make_date_dtype();
    }

    throw std::runtime_error("could not deduce pydynd dtype from the python object");
}

/**
 * Creates a dynd::dtype out of typical Python typeobjects.
 */
static dynd::dtype make_dtype_from_pytypeobject(PyTypeObject* obj)
{
    if (obj == &PyBool_Type) {
        return make_dtype<dynd_bool>();
    } else if (obj == &PyInt_Type || obj == &PyLong_Type) {
        return make_dtype<int32_t>();
    } else if (obj == &PyFloat_Type) {
        return make_dtype<double>();
    } else if (obj == &PyComplex_Type) {
        return make_dtype<complex<double> >();
    } else if (PyObject_IsSubclass((PyObject *)obj, ctypes.PyCData_Type)) {
        // CTypes type object
        return dtype_from_ctypes_cdatatype((PyObject *)obj);
    } else if (obj == PyDateTimeAPI->DateType) {
        return make_date_dtype();
    }

    throw std::runtime_error("could not convert the given Python TypeObject into a dynd::dtype");
}

dynd::dtype pydynd::make_dtype_from_object(PyObject* obj)
{
    if (WDType_Check(obj)) {
        return ((WDType *)obj)->v;
    } else if (PyString_Check(obj)) {
        char *s = NULL;
        Py_ssize_t len = 0;
        if (PyString_AsStringAndSize(obj, &s, &len) < 0) {
            throw std::runtime_error("error processing string input to make dynd::dtype");
        }
        return dtype(string(s, len));
    } else if (PyUnicode_Check(obj)) {
        // TODO: Haven't implemented unicode yet.
        throw std::runtime_error("unicode to dynd::dtype conversion isn't implemented yet");
    } else if (PyType_Check(obj)) {
#if DYND_NUMPY_INTEROP
        dtype result;
        if (dtype_from_numpy_scalar_typeobject((PyTypeObject *)obj, result) == 0) {
            return result;
        }
#endif // DYND_NUMPY_INTEROP
        return make_dtype_from_pytypeobject((PyTypeObject *)obj);
    }

#if DYND_NUMPY_INTEROP
    if (PyArray_DescrCheck(obj)) {
        return dtype_from_numpy_dtype((PyArray_Descr *)obj);
    }
#endif // DYND_NUMPY_INTEROP

    stringstream ss;
    ss << "could not convert the object ";
    pyobject_ownref repr(PyObject_Repr(obj));
    ss << PyString_AsString(repr.get());
    ss << " into a dynd::dtype";
    throw std::runtime_error(ss.str());
}

PyObject *pydynd::dtype_as_pyobject(const dynd::dtype& dt)
{
    pyobject_ownref obj(_PyObject_New(WDType_Type));
    ((WDType *)obj.get())->v = dt;
    return obj.release();
}

static string_encoding_t encoding_from_pyobject(PyObject *encoding_obj)
{
    string_encoding_t encoding = string_encoding_invalid;
    if (PyString_Check(encoding_obj)) {
        char *s = NULL;
        Py_ssize_t len = 0;
        if (PyString_AsStringAndSize(encoding_obj, &s, &len) < 0) {
            throw std::runtime_error("error processing string input to process string encoding");
        }
        switch (len) {
        case 5:
            switch (s[1]) {
            case 'c':
                if (strcmp(s, "ucs_2") == 0) {
                    encoding = string_encoding_ucs_2;
                }
                break;
            case 's':
                if (strcmp(s, "ascii") == 0) {
                    encoding = string_encoding_ascii;
                }
                break;
            case 't':
                if (strcmp(s, "utf_8") == 0) {
                    encoding = string_encoding_utf_8;
                }
                break;
            }
            break;
        case 6:
            switch (s[4]) {
            case '1':
                if (strcmp(s, "utf_16") == 0) {
                    encoding = string_encoding_utf_16;
                }
                break;
            case '3':
                if (strcmp(s, "utf_32") == 0) {
                    encoding = string_encoding_utf_32;
                }
                break;
            }
        }

        if (encoding != string_encoding_invalid) {
            return encoding;
        } else {
            stringstream ss;
            ss << "invalid input \"" << s << "\"for string encoding";
            throw std::runtime_error(ss.str());
        }

    } else if (PyUnicode_Check(encoding_obj)) {
        // TODO: Haven't implemented unicode yet.
        throw std::runtime_error("unicode isn't implemented yet for determining string encodings");
    } else {
        throw std::runtime_error("invalid input type for string encoding");
    }
}

dynd::dtype pydynd::dnd_make_convert_dtype(const dynd::dtype& to_dtype, const dynd::dtype& from_dtype, PyObject *errmode)
{
    return make_convert_dtype(to_dtype, from_dtype, pyarg_error_mode(errmode));
}

dynd::dtype pydynd::dnd_make_fixedstring_dtype(PyObject *encoding_obj, intptr_t size)
{
    string_encoding_t encoding = encoding_from_pyobject(encoding_obj);

    return make_fixedstring_dtype(encoding, size);
}

dynd::dtype pydynd::dnd_make_string_dtype(PyObject *encoding_obj)
{
    string_encoding_t encoding = encoding_from_pyobject(encoding_obj);

    return make_string_dtype(encoding);
}

dynd::dtype pydynd::dnd_make_pointer_dtype(const dtype& target_dtype)
{
    return make_pointer_dtype(target_dtype);
}

dynd::dtype pydynd::dnd_make_struct_dtype(PyObject *field_types, PyObject *field_names)
{
    vector<dtype> field_types_vec;
    vector<string> field_names_vec;
    pyobject_as_vector_dtype(field_types, field_types_vec);
    pyobject_as_vector_string(field_names, field_names_vec);
    return make_struct_dtype(field_types_vec, field_names_vec);
}

dynd::dtype pydynd::dnd_make_fixedstruct_dtype(PyObject *field_types, PyObject *field_names)
{
    vector<dtype> field_types_vec;
    vector<string> field_names_vec;
    pyobject_as_vector_dtype(field_types, field_types_vec);
    pyobject_as_vector_string(field_names, field_names_vec);
    return make_fixedstruct_dtype(field_types_vec, field_names_vec);
}

dynd::dtype pydynd::dnd_make_fixedarray_dtype(const dtype& element_dtype, PyObject *shape, PyObject *axis_perm)
{
    vector<intptr_t> shape_vec;
    if (PySequence_Check(shape)) {
        pyobject_as_vector_intp(shape, shape_vec);
    } else {
        shape_vec.push_back(pyobject_as_index(shape));
    }

    if (axis_perm != Py_None) {
        vector<int> axis_perm_vec;
        pyobject_as_vector_int(axis_perm, axis_perm_vec);
        if (!is_valid_perm((int)axis_perm_vec.size(), axis_perm_vec.empty() ? NULL : &axis_perm_vec[0])) {
            throw runtime_error("Provided axis_perm is not a valid permutation");
        }
        if (axis_perm_vec.size() != shape_vec.size()) {
            throw runtime_error("Provided axis_perm is a different size than the provided shape");
        }
        return make_fixedarray_dtype(element_dtype, (int)shape_vec.size(), &shape_vec[0], &axis_perm_vec[0]);
    } else {
        return make_fixedarray_dtype(element_dtype, (int)shape_vec.size(), &shape_vec[0], NULL);
    }
}

dynd::dtype pydynd::dnd_make_date_dtype(PyObject *unit)
{
    date_unit_t u = (date_unit_t)pyarg_strings_to_int(unit, "unit", date_unit_day, "day", date_unit_day,
                    "month", date_unit_month, "year", date_unit_year);
    return make_date_dtype(u);
}

dynd::dtype pydynd::dtype_getitem(const dynd::dtype& d, PyObject *subscript)
{
    // Convert the pyobject into an array of iranges
    intptr_t size;
    shortvector<irange> indices;
    if (!PyTuple_Check(subscript)) {
        // A single subscript
        size = 1;
        indices.init(1);
        indices[0] = pyobject_as_irange(subscript);
    } else {
        size = PyTuple_GET_SIZE(subscript);
        // Tuple of subscripts
        indices.init(size);
        for (Py_ssize_t i = 0; i < size; ++i) {
            indices[i] = pyobject_as_irange(PyTuple_GET_ITEM(subscript, i));
        }
    }

    // Do an indexing operation
    return d.at_array((int)size, indices.get());
}
