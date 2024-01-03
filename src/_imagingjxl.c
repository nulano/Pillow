#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "libImaging/Imaging.h"
#include <jxl/decode.h>

typedef struct {
    PyObject_HEAD JxlDecoder *decoder;
    Py_buffer buffer;
    size_t frame_no;
    PyObject *info;
} JxlDecoderObject;

int _jxl_decoder_reset(JxlDecoderObject *self, int events_wanted) {
    self->frame_no = 0;
    JxlDecoderRewind(self->decoder);
    if (JxlDecoderSubscribeEvents(self->decoder, events_wanted) != JXL_DEC_SUCCESS) {
        goto err;
    }
    if (JxlDecoderSetInput(self->decoder, self->buffer.buf, self->buffer.len) != JXL_DEC_SUCCESS) {
        goto err;
    }
    JxlDecoderCloseInput(self->decoder);
    return 0;

err:
    PyErr_SetString(PyExc_OSError, "failed to set jxl decoder input");
    return 1;
}

int _jxl_decoder_rewind(JxlDecoderObject *self) {
    return _jxl_decoder_reset(self, JXL_DEC_COLOR_ENCODING | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE);
}

PyObject *jxl_decoder_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    Py_buffer buffer;
    static char *kwlist[] = {"data", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "y*:JxlDecoder", kwlist, &buffer)) {
        return NULL;
    }

    JxlDecoderObject *self = (JxlDecoderObject *)subtype->tp_alloc(subtype, 0);
    if (self) {
        self->decoder = NULL;
        self->buffer = buffer;
        self->info = NULL;

        self->decoder = JxlDecoderCreate(0);
        if (!self->decoder) {
            goto decoder_err;
        }
// TODO do we want to keep orientation?
//        if (JxlDecoderSetKeepOrientation(self->decoder, JXL_TRUE) != JXL_DEC_SUCCESS) {
//            goto decoder_err;
//        }
        if (JxlDecoderSetUnpremultiplyAlpha(self->decoder, JXL_TRUE) != JXL_DEC_SUCCESS) {
            goto decoder_err;
        }
        if (_jxl_decoder_rewind(self)) {
            Py_DECREF(self);
            return NULL;
        }
    }
    return self;

decoder_err:
    Py_DECREF(self);
    PyErr_SetString(PyExc_RuntimeError, "failed to create jxl decoder");
    return NULL;
}

PyObject *jxl_decoder_get_info(JxlDecoderObject *self, PyObject *Py_UNUSED(ignored)) {
    if (self->info) {
        Py_INCREF(self->info);
        return self->info;
    }

    Py_ssize_t num_frames = 0;

    PyObject *box_types = PyList_New(0);
    if (!box_types) {
        return NULL;
    }

    if (_jxl_decoder_reset(self, JXL_DEC_BOX | JXL_DEC_BASIC_INFO | JXL_DEC_FRAME)) {
        return NULL;
    }

    JxlBasicInfo info;

    JxlDecoderStatus status = JXL_DEC_NEED_MORE_INPUT;
    while (status != JXL_DEC_SUCCESS) {
        status = JxlDecoderProcessInput(self->decoder);
        switch (status) {
            case JXL_DEC_BOX: {
                JxlBoxType box_type;
                if (JxlDecoderGetBoxType(self->decoder, box_type, JXL_TRUE) != JXL_DEC_SUCCESS) {
                    goto jxl_err;
                }
                PyObject *type_obj = PyBytes_FromStringAndSize(box_type, (Py_ssize_t)sizeof(box_type));
                if (!type_obj) {
                    goto err;
                }
                if (PyList_Append(box_types, type_obj) < 0) {
                    Py_DECREF(type_obj);
                    goto err;
                }
                break;
            }
            case JXL_DEC_BASIC_INFO:
                if (JxlDecoderGetBasicInfo(self->decoder, &info) != JXL_DEC_SUCCESS) {
                    goto jxl_err;
                }
                break;
            case JXL_DEC_FRAME:
                num_frames++;
                break;
            case JXL_DEC_SUCCESS:
                break;
            default:
                PyErr_Format(PyExc_RuntimeError, "unexpected result from jxl decoder: %d", status);
                goto err;
        }
    }

    if (_jxl_decoder_rewind(self)) {
        goto err;
    }

    PyObject *preview_size;
    if (info.have_preview) {
        preview_size = Py_BuildValue("(II)", info.preview.xsize, info.preview.ysize);
    } else {
        Py_INCREF(Py_None);
        preview_size = Py_None;
    }
    if (!preview_size) {
        goto err;
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
        goto err;
    }

    self->info = Py_BuildValue("{s(II)sIsNsNsNsisIsIsIsnsN}",
            "size", info.xsize, info.ysize,
            "bits_per_sample", info.bits_per_sample,
            "uses_original_profile", PyBool_FromLong(info.uses_original_profile),
            "preview_size", preview_size,
            "animation_info", animation_info,
            "orientation", (int) info.orientation,
            "num_color_channels", info.num_color_channels,
            "num_extra_channels", info.num_extra_channels,
            "alpha_bits", info.alpha_bits,
            "num_frames", num_frames,
            "box_types", box_types);
    if (!self->info) {
        goto err;
    }
    Py_INCREF(self->info);
    return self->info;

jxl_err:
    PyErr_SetString(PyExc_OSError, "failed to get jxl info");
err:
    Py_XDECREF(box_types);
    Py_XDECREF(preview_size);
    Py_XDECREF(animation_info);
    return NULL;
}

PyObject *jxl_decoder_get_icc_profile(JxlDecoderObject *self, PyObject *Py_UNUSED(ignored)) {
    Py_ssize_t icc_size = 0;
    JxlDecoderStatus status = JxlDecoderGetICCProfileSize(self->decoder, JXL_COLOR_PROFILE_TARGET_DATA, &icc_size);
    while (status == JXL_DEC_NEED_MORE_INPUT) {
        status = JxlDecoderProcessInput(self->decoder);
        switch (status) {
            case JXL_DEC_COLOR_ENCODING:
                break;
            default:  /* icc profile should be the first subscribed event */
            case JXL_DEC_NEED_MORE_INPUT:
            case JXL_DEC_ERROR:
                PyErr_Format(PyExc_RuntimeError, "unexpected result from jxl decoder: %d", status);
                return NULL;
        }
    }
    status = JxlDecoderGetICCProfileSize(self->decoder, JXL_COLOR_PROFILE_TARGET_DATA, &icc_size);
    if (status != JXL_DEC_SUCCESS || icc_size <= 0) {
        /* either missing or incompatible */
        Py_RETURN_NONE;
    }

    PyObject *icc_data = PyBytes_FromStringAndSize(NULL, icc_size);
    if (!icc_data) {
        return NULL;
    }
    if (JxlDecoderGetColorAsICCProfile(self->decoder, JXL_COLOR_PROFILE_TARGET_DATA,
                                       PyBytes_AS_STRING(icc_data), icc_size) != JXL_DEC_SUCCESS) {
        Py_DECREF(icc_data);
        PyErr_SetString(PyExc_RuntimeError, "failed to get icc color profile");
        return NULL;
    }
    return icc_data;
}

void _jxl_decoder_image_out_callback_RGB(Imaging im, size_t x, size_t y, size_t num_pixels, const UINT8 *pixels) {
    if (x >= im->xsize || y >= im->ysize) {
        return;  /* TODO set error? */
    }
    if (num_pixels >= im->xsize - x) {
        num_pixels = im->xsize - x;
    }
    size_t w = x + num_pixels;
    UINT8 *row = (UINT8 *)im->image[y] + x * 4;
    for (; x < w; x++) {
        row[0] = pixels[0];
        row[1] = pixels[1];
        row[2] = pixels[2];
        row[3] = 0xff;
        row += 4;
        pixels += 3;
    }
}

PyObject *jxl_decoder_next(JxlDecoderObject *self, PyObject *arg) {
    Py_ssize_t id = PyLong_AsSsize_t(arg);
    if (PyErr_Occurred()) {
        return NULL;
    }

    /* TODO figure out format and callback */
    JxlPixelFormat format = {3, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
    JxlImageOutCallback callback = (JxlImageOutCallback)&_jxl_decoder_image_out_callback_RGB;

    JxlFrameHeader header;
    PyObject *frame_name = NULL;
    JxlDecoderStatus status = JXL_DEC_NEED_MORE_INPUT;
    int progress = 0;
    while (status != JXL_DEC_FULL_IMAGE) {
        status = JxlDecoderProcessInput(self->decoder);
        switch (status) {
            case JXL_DEC_FRAME:
                if (progress != 0) {
                    goto err;
                }
                progress = 1;
                self->frame_no += 1;
                if (JxlDecoderGetFrameHeader(self->decoder, &header) != JXL_DEC_SUCCESS) {
                    PyErr_SetString(PyExc_RuntimeError, "failed to get jxl frame header");
                    return NULL;
                }
                frame_name = PyBytes_FromStringAndSize(NULL, header.name_length);
                if (!frame_name) {
                    return NULL;
                }
                if (JxlDecoderGetFrameName(self->decoder, PyBytes_AS_STRING(frame_name), header.name_length + 1) != JXL_DEC_SUCCESS) {
                    Py_DECREF(frame_name);
                    PyErr_SetString(PyExc_RuntimeError, "failed to get jxl frame name");
                    return NULL;
                }
                break;
            case JXL_DEC_NEED_IMAGE_OUT_BUFFER:
                if (progress != 1) {
                    goto err;
                }
                progress = 2;
                if (JxlDecoderSetImageOutCallback(self->decoder, &format, callback, (void *)id) != JXL_DEC_SUCCESS) {
                    goto err;
                }
                break;
            case JXL_DEC_FULL_IMAGE:
                if (progress != 2) {
                    goto err;
                }
                progress = 3;
                break;  /* we are done */
            case JXL_DEC_BOX:  /* TODO ignoring this will discard metadata, is that an issue? */
            case JXL_DEC_SUCCESS:
                Py_XDECREF(frame_name);
                Py_RETURN_NONE;  /* no more data */
            default:  /* TODO can this infinitely loop? probably not, since we'll get success? */
                /*break;*/  /* unknown event, continue */
            case JXL_DEC_NEED_MORE_INPUT:
            case JXL_DEC_ERROR:
err:
                Py_XDECREF(frame_name);
                PyErr_Format(PyExc_RuntimeError, "unexpected result from jxl decoder: %d", status);
                return NULL;
        }
    }

    return Py_BuildValue("{sIsIsNsN}",
            "duration", header.duration,
            "timecode", header.timecode,
            "name", frame_name,
            "is_last", PyBool_FromLong(header.is_last));
}

PyObject *jxl_decoder_skip(JxlDecoderObject *self, PyObject *skip_obj) {
    size_t skip = PyLong_AsSize_t(skip_obj);
    if (PyErr_Occurred()) {
        return NULL;
    }
    JxlDecoderSkipFrames(self->decoder, skip);
    self->frame_no += skip;
    Py_RETURN_NONE;
}

PyObject *jxl_decoder_rewind(JxlDecoderObject *self, PyObject *Py_UNUSED(ignored)) {
    if (_jxl_decoder_rewind(self)) {
        return NULL;
    }
    Py_RETURN_NONE;
}

PyObject *jxl_decoder_frame_no(JxlDecoderObject *self, void *closure) {
    return PyLong_FromSize_t(self->frame_no);
}

/*
TEST function
void _jxl_decoder_dummy_callback(Imaging im, size_t x, size_t y, size_t num_pixels, const UINT8 *pixels) {
}

PyObject *jxl_decoder_proc(JxlDecoderObject *self, PyObject *unused) {
    JxlDecoderStatus status = JxlDecoderProcessInput(self->decoder);
    if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
        JxlPixelFormat format = {3, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
        JxlImageOutCallback callback = (JxlImageOutCallback)&_jxl_decoder_dummy_callback;
        JxlDecoderSetImageOutCallback(self->decoder, &format, callback, NULL);
    } else if (status == JXL_DEC_BOX) {
        JxlBoxType type;
        JxlDecoderGetBoxType(self->decoder, type, 1);
        return Py_BuildValue("iy#", (int)status, type, (Py_ssize_t)4);
    }
    return Py_BuildValue("i", (int)status);
}
*/

void jxl_decoder_dealloc(JxlDecoderObject *self) {
    if (self->decoder) {
        JxlDecoderDestroy(self->decoder);
    }
    PyBuffer_Release(&self->buffer);
    Py_XDECREF(self->info);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static struct PyMethodDef jxl_decoder_methods[] = {
    {"get_info", (PyCFunction)jxl_decoder_get_info, METH_NOARGS, "Get basic JXL info"},
    {"get_icc_profile", (PyCFunction)jxl_decoder_get_icc_profile, METH_NOARGS, "Get Target ICC profile"},
    /*{"proc", (PyCFunction)jxl_decoder_proc, METH_NOARGS, "return next event number"},*/
    {"next", (PyCFunction)jxl_decoder_next, METH_O, "Return next image frame data"},
    {"skip", (PyCFunction)jxl_decoder_skip, METH_O, "Skip requested number of frames"},
    {"rewind", (PyCFunction)jxl_decoder_rewind, METH_NOARGS, "Rewind to the first frame"},
    {NULL, NULL} /* sentinel */
};

static struct PyGetSetDef jxl_decoder_getset[] = {
    {"frame_no", (getter)jxl_decoder_frame_no, 0, "Index of the next frame to be read by next() method"},
    {NULL}};

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
    jxl_decoder_getset,                /*tp_getset*/
    0,                                 /*tp_base*/
    0,                                 /*tp_dict*/
    0,                                 /*tp_descr_get*/
    0,                                 /*tp_descr_set*/
    0,                                 /*tp_dictoffset*/
    0,                                 /*tp_init*/
    0,                                 /*tp_alloc*/
    jxl_decoder_new,                   /*tp_new*/
};

PyObject *jxl_check_signature(PyObject *mod, PyObject *prefix) {
    char *buffer;
    Py_ssize_t length;
    if (PyBytes_AsStringAndSize(prefix, &buffer, &length) < 0) {
        return NULL;
    }
    JxlSignature signature = JxlSignatureCheck(buffer, length);
    switch (signature) {
        case JXL_SIG_NOT_ENOUGH_BYTES:
            PyErr_SetString(PyExc_OSError, "not enough bytes to determine file type");
            return NULL;
        case JXL_SIG_INVALID:
            Py_RETURN_NONE;
        case JXL_SIG_CODESTREAM:
            return Py_BuildValue("s", "codestream");
        case JXL_SIG_CONTAINER:
            return Py_BuildValue("s", "container");
        default:
            PyErr_Format(PyExc_RuntimeError, "unexpected result from jxl decoder: %d", (int)signature);
            return NULL;
    }
}

static PyMethodDef jxl_methods[] = {
    {"check_signature", jxl_check_signature, METH_O, "Check signature to determine JXL file type, or return None"},
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
        jxl_methods,    /* m_methods */
    };

    m = PyModule_Create(&module_def);
    if (setup_module(m) < 0) {
        Py_DECREF(m);
        return NULL;
    }

    return m;
}
