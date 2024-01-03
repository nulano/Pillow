#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "libImaging/Imaging.h"
#include <jxl/decode.h>

typedef struct {
    PyObject_HEAD JxlDecoder *decoder;
    PyObject *fp;
    PyObject *data;
} JxlDecoderObject;

PyObject *jxl_decoder_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    JxlDecoderObject *self = (JxlDecoderObject *)subtype->tp_alloc(subtype, 0);
    if (self) {
        self->decoder = NULL;
        self->fp = NULL;
        self->data = NULL;

        static char *kwlist[] = {"fp", NULL};
        if (!PyArg_ParseTupleAndKeywords(args, kwds, "O:JxlDecoder", kwlist, &self->fp)) {
            Py_DECREF(self);
            return NULL;
        }
        Py_INCREF(self->fp);

        self->decoder = JxlDecoderCreate(0);
        if (!self->decoder) {
            goto decoder_err;
        }
        if (JxlDecoderSubscribeEvents(self->decoder, JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING /* TODO list others */) != JXL_DEC_SUCCESS) {
            goto decoder_err;
        }
// TODO do we want to keep orientation?
//        if (JxlDecoderSetKeepOrientation(self->decoder, 1) != JXL_DEC_SUCCESS) {
//            goto decoder_err;
//        }
        if (JxlDecoderSetUnpremultiplyAlpha(self->decoder, 1) != JXL_DEC_SUCCESS) {
            goto decoder_err;
        }
    }
    return self;

decoder_err:
    Py_DECREF(self);
    PyErr_SetString(PyExc_RuntimeError, "failed to create jxl decoder");
    return NULL;
}

int _jxl_decoder_seek(JxlDecoderObject *self, Py_ssize_t offset) {
    PyObject *result;
    result = PyObject_CallMethod(self->fp, "seek", "ni", offset, 1);  /* TODO support whence != io.SEEK_CUR? */
    Py_XDECREF(result);
    return !result;
}

Py_ssize_t _jxl_decoder_read(JxlDecoderObject *self, Py_ssize_t len) {
    Py_ssize_t rewind;
    char *buffer;
    Py_ssize_t length_read;
    JxlDecoderStatus status;

    rewind = JxlDecoderReleaseInput(self->decoder);
    Py_XDECREF(self->data);
    if (_jxl_decoder_seek(self, -rewind)) {
        goto err;
    }
    len += rewind;

    self->data = PyObject_CallMethod(self->fp, "read", "n", len);
    if (!self->data) {
        goto err;
    }
    int result = PyBytes_AsStringAndSize(self->data, &buffer, &length_read);
    if (result < 0) {
        goto err;
    }
    if (length_read <= rewind || length_read > len) {
        PyErr_SetString(PyExc_RuntimeError, "fp.read returned bytes with unexpected length");
        goto err;
    }

    status = JxlDecoderSetInput(self->decoder, buffer, length_read);
    if (status != JXL_DEC_SUCCESS) {
        goto err;
    }
    return length_read - rewind;

err:
    return -1;
}

PyObject *jxl_decoder_get_info(JxlDecoderObject *self, PyObject *Py_UNUSED(ignored)) {
    JxlDecoderStatus status = JxlDecoderGetBasicInfo(self->decoder, NULL);
    while (status == JXL_DEC_NEED_MORE_INPUT) {
        status = JxlDecoderProcessInput(self->decoder);
        switch (status) {
            case JXL_DEC_BASIC_INFO:
                break;
            case JXL_DEC_NEED_MORE_INPUT:
                size_t size = JxlDecoderSizeHintBasicInfo(self->decoder);
                if (_jxl_decoder_read(self, size) < 0) {
                    return NULL;
                }
                break;
            case JXL_DEC_SUCCESS:
                /* break; TODO this status seems unexpected */
            case JXL_DEC_ERROR:
            default:
                /* TODO use Pillow error? */
                PyErr_SetString(PyExc_RuntimeError, "unexpected result from jxl decoder");
                return NULL;
        }
    }
    JxlBasicInfo info;
    status = JxlDecoderGetBasicInfo(self->decoder, &info);
    if (status != JXL_DEC_SUCCESS) {
        PyErr_SetString(PyExc_RuntimeError, "failed to get basic info");
        return NULL;
    }

    PyObject *preview_size;
    if (info.have_preview) {
        preview_size = Py_BuildValue("(II)", info.preview.xsize, info.preview.ysize);
    } else {
        Py_INCREF(Py_None);
        preview_size = Py_None;
    }
    if (!preview_size) {
        return NULL;
    }

    PyObject *animation_info;
    if (info.have_preview) {
        animation_info = Py_BuildValue("{s(II)sIsN}",
                "tps", info.animation.tps_numerator, info.animation.tps_denominator,
                "num_loops", info.animation.num_loops,
                "have_timecodes", PyBool_FromLong(info.animation.have_timecodes));
    } else {
        Py_INCREF(Py_None);
        animation_info = Py_None;
    }
    if (!animation_info) {
        Py_DECREF(preview_size);
        return NULL;
    }

    return Py_BuildValue("{s(II)sIsNsNsNsisIsIsI}",
            "size", info.xsize, info.ysize,
            "bits_per_sample", info.bits_per_sample,
            "uses_original_profile", PyBool_FromLong(info.uses_original_profile),
            "preview_size", preview_size,
            "animation_info", animation_info,
            "orientation", (int) info.orientation,
            "num_color_channels", info.num_color_channels,
            "num_extra_channels", info.num_extra_channels,
            "alpha_bits", info.alpha_bits
            /* TODO anything else needed? e.g. preview size, animation params */);
}

PyObject *jxl_decoder_get_icc_profile(JxlDecoderObject *self, PyObject *Py_UNUSED(ignored)) {
    size_t icc_size = 0;
    JxlDecoderStatus status = JxlDecoderGetICCProfileSize(self->decoder, JXL_COLOR_PROFILE_TARGET_DATA, &icc_size);
    while (status == JXL_DEC_NEED_MORE_INPUT) {
        status = JxlDecoderProcessInput(self->decoder);
        switch (status) {
            case JXL_DEC_BASIC_INFO:
                /* continue, icc profile comes later */
                status = JXL_DEC_NEED_MORE_INPUT;
                break;
            default: /* different event, probably have no icc profile, fall through */
            case JXL_DEC_COLOR_ENCODING:
                break;
            case JXL_DEC_NEED_MORE_INPUT:
                if (_jxl_decoder_read(self, 1024 /* TODO */) < 0) {
                    return NULL;
                }
                break;
            case JXL_DEC_ERROR:
                /* TODO use Pillow error? */
                PyErr_SetString(PyExc_RuntimeError, "unexpected result from jxl decoder");
                return NULL;
        }
    }
    status = JxlDecoderGetICCProfileSize(self->decoder, JXL_COLOR_PROFILE_TARGET_DATA, &icc_size);
    if (status != JXL_DEC_SUCCESS || (Py_ssize_t) icc_size <= 0) { /* TODO is this check correct? */
        /* either missing or incompatible */
        Py_RETURN_NONE;
    }

    PyObject *icc_data = PyBytes_FromStringAndSize(NULL, icc_size);
    if (!icc_data) {
        return NULL;
    }
    char *buffer = PyBytes_AS_STRING(icc_data);
    if (JxlDecoderGetColorAsICCProfile(self->decoder, JXL_COLOR_PROFILE_TARGET_DATA, buffer, icc_size) != JXL_DEC_SUCCESS) {
        Py_DECREF(icc_data);
        PyErr_SetString(PyExc_RuntimeError, "failed to get icc color profile");
        return NULL;
    }
    return icc_data;
}

void jxl_decoder_dealloc(JxlDecoderObject *self) {
    if (self->decoder) {
        JxlDecoderDestroy(self->decoder);
    }
    Py_XDECREF(self->data);
    Py_XDECREF(self->fp);
    Py_TYPE(self)->tp_free((PyObject *)self);
}


static struct PyMethodDef jxl_decoder_methods[] = {
    {"get_info", (PyCFunction)jxl_decoder_get_info, METH_NOARGS, "Get basic JXL info"},
    {"get_icc_profile", (PyCFunction)jxl_decoder_get_icc_profile, METH_NOARGS, "Get Target ICC profile"},
    {NULL, NULL} /* sentinel */
};


static PyTypeObject JxlDecoder_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "JxlDecoder",      /*tp_name */
    sizeof(JxlDecoderObject),                         /*tp_basicsize */
    0,                                                /*tp_itemsize */
    /* methods */
    (destructor)jxl_decoder_dealloc,   /*tp_dealloc*/
    0,                                 /*tp_vectorcall_offset*/
    0,                                 /*tp_getattr*/
    0,                                 /*tp_setattr*/
    0,                                 /*tp_as_async*/
    0,                                 /*tp_repr*/
    0,                                 /*tp_as_number*/
    0,                                 /*tp_as_sequence*/
    0,                                 /*tp_as_mapping*/
    0,                                 /*tp_hash*/
    0,                                 /*tp_call*/
    0,                                 /*tp_str*/
    0,                                 /*tp_getattro*/
    0,                                 /*tp_setattro*/
    0,                                 /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,                /*tp_flags*/
    0,                                 /*tp_doc*/
    0,                                 /*tp_traverse*/
    0,                                 /*tp_clear*/
    0,                                 /*tp_richcompare*/
    0,                                 /*tp_weaklistoffset*/
    0,                                 /*tp_iter*/
    0,                                 /*tp_iternext*/
    jxl_decoder_methods,               /*tp_methods*/
    0,                                 /*tp_members*/
    0,                                 /*tp_getset*/
    0,                                 /*tp_base*/
    0,                                 /*tp_dict*/
    0,                                 /*tp_descr_get*/
    0,                                 /*tp_descr_set*/
    0,                                 /*tp_dictoffset*/
    0,                                 /*tp_init*/
    0,                                 /*tp_alloc*/
    jxl_decoder_new,                   /*tp_new*/
};

static PyMethodDef jxlMethods[] = {
    {NULL, NULL}};


static int
setup_module(PyObject *m) {
    PyObject *d = PyModule_GetDict(m);

    if (PyType_Ready(&JxlDecoder_Type) < 0)
        return NULL;

    Py_INCREF(&JxlDecoder_Type);
    if (PyModule_AddObject(m, "JxlDecoder", (PyObject *) &JxlDecoder_Type) < 0) {
        Py_DECREF(&JxlDecoder_Type);
        Py_DECREF(m);
        return NULL;
    }

    uint32_t vn = JxlDecoderVersion();
    PyObject *v = PyUnicode_FromFormat("%d.%d.%d", vn / 1000000, (vn / 1000) % 1000, vn % 1000);
    PyDict_SetItemString(d, "libjxl_version", v ? v : Py_None);

    Py_XDECREF(v);

    return 0;
}


PyMODINIT_FUNC
PyInit__imagingjxl(void) {
    PyObject *m;

    static PyModuleDef module_def = {
        PyModuleDef_HEAD_INIT,
        "_imagingjxl", /* m_name */
        NULL,          /* m_doc */
        -1,            /* m_size */
        jxlMethods,    /* m_methods */
    };

    m = PyModule_Create(&module_def);
    if (setup_module(m) < 0) {
        Py_DECREF(m);
        return NULL;
    }

    return m;
}
