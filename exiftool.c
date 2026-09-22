#include "exiftool.h"
#define _CRT_NONSTDC_NO_DEPRECATE
#include <string.h>
#include <stdio.h>

#include <EXTERN.h>
#include <perl.h>


/* defined in packed.obj by bin2obj */
#ifndef __linux__
extern int _packed_pl_size;
extern char _packed_pl[];
#else
//The size field from objcopy causes a segfault if accessed, so we need to use the start and end instead
//Some googling indicates this is likely from compiling with -fPIC
extern char _binary_packed_pl_start[];
extern char _binary_packed_pl_end[];
#endif

void xs_init(pTHX);

static exifdata_t unwrap_ref(SV *ref) {
    int type = SvROK(ref) ? SvTYPE(SvRV(ref)) : 0;
    int unwrap = type == SVt_PVAV || type == SVt_PVHV;
    return unwrap ? SvRV(ref) : ref;
}

static SV *wrap_ref(exiftool_t tool, exifdata_t value) {
    dTHXa(tool);
    int type = SvTYPE(value);
    int wrap = type == SVt_PVAV || type == SVt_PVHV;
    return wrap ? newRV_noinc(value) : value;
}

/* === exiftool functions === */

exiftool_t exiftool_Create(void) {
    int argc = 3;
    char *argv[] = { "", "-e", "0" };

    static int sys_init = 0;
    if (!sys_init) {
        PERL_SYS_INIT(&argc, (char***)&argv);
        sys_init = 1;
    }

    exiftool_t tool = perl_alloc();
    dTHXa(tool);
    PERL_SET_CONTEXT(tool);
    perl_construct(tool);
    perl_parse(tool, xs_init, argc, argv, NULL);

#ifndef __linux__
    SV *code = newSVpv(_packed_pl, _packed_pl_size);
#else
    SV *code = newSVpv(_binary_packed_pl_start, (_binary_packed_pl_end - _binary_packed_pl_start));
#endif
    sv_2mortal(code);
    eval_sv(code, G_SCALAR | G_RETHROW);
    return tool;
}

void exiftool_Destroy(exiftool_t tool) {
    PERL_SET_CONTEXT(tool);

    perl_destruct(tool);
    perl_free(tool);
}

exifdata_t exiftool_Options(exiftool_t tool, exifdata_t options) {
    dTHXa(tool);
    PERL_SET_CONTEXT(tool);
    dSP;

    int has_options = options && SvTYPE(options) == SVt_PVAV;
    int num_options = has_options ? (int)av_top_index(options) + 1 : 0;
    PUSHMARK(SP);
    EXTEND(SP, 1 + num_options);
    PUSHs(get_sv("exifTool", 0));
    for (int i = 0; i < num_options; ++i)
        PUSHs(*av_fetch((AV*)options, i, 0));
    PUTBACK;

    int ok = call_method("Options", G_SCALAR);
    SPAGAIN;
    SV *value = ok ? POPs : newSV(0);
    if (ok) value = SvREFCNT_inc(unwrap_ref(value));
    PUTBACK;
    return value;
}

void exiftool_ClearOptions(exiftool_t tool) {
    dTHXa(tool);
    PERL_SET_CONTEXT(tool);
    dSP;

    PUSHMARK(SP);
    XPUSHs(get_sv("exifTool", 0));
    PUTBACK;

    call_method("ClearOptions", G_SCALAR);
}

int exiftool_ExtractInfo(exiftool_t tool, const char *filename, exifdata_t tags) {
    dTHXa(tool);
    PERL_SET_CONTEXT(tool);
    dSP;

    SV *file = newSVpv(filename, 0);
    int has_tags = tags && SvTYPE(tags) == SVt_PVAV;
    SV *ref = has_tags ? newRV_inc(tags) : newSV(0);
    PUSHMARK(SP);
    EXTEND(SP, 2 + has_tags);
    PUSHs(get_sv("exifTool", 0));
    PUSHs(file);
    if (has_tags) PUSHs(ref);
    PUTBACK;

    int ok = call_method("ExtractInfo", G_SCALAR);
    SPAGAIN;
    int success = ok ? (int)POPi : 0;
    PUTBACK;
    SvREFCNT_dec(ref);
    SvREFCNT_dec(file);
    return success;
}

exifdata_t exiftool_GetInfo(exiftool_t tool, exifdata_t tags) {
    dTHXa(tool);
    PERL_SET_CONTEXT(tool);
    dSP;

    int has_tags = tags && SvTYPE(tags) == SVt_PVAV;
    SV *ref = has_tags ? newRV_inc(tags) : newSV(0);
    PUSHMARK(SP);
    EXTEND(SP, 1 + has_tags);
    PUSHs(get_sv("exifTool", 0));
    if (has_tags) PUSHs(ref);
    PUTBACK;

    int ok = call_method("GetInfo", G_SCALAR);
    SPAGAIN;
    SV *info = ok ? POPs : newSV(0);
    if (ok) info = SvREFCNT_inc(SvRV(info));
    PUTBACK;
    SvREFCNT_dec(ref);
    return info;
}

exifdata_t exiftool_GetTagList(exiftool_t tool, exifdata_t info) {
    dTHXa(tool);
    PERL_SET_CONTEXT(tool);
    dSP;

    int has_info = info && SvTYPE(info) == SVt_PVHV;
    SV *ref = has_info ? newRV_inc(info) : newSV(0);
    PUSHMARK(SP);
    EXTEND(SP, 1 + has_info);
    PUSHs(get_sv("exifTool", 0));
    if (has_info) PUSHs(ref);
    PUTBACK;

    int count = call_method("GetTagList", G_LIST);
    SPAGAIN;
    AV *tags = newAV();
    av_extend(tags, count);
    for (int i = count; i-- > 0;)
       av_store(tags, i, SvREFCNT_inc(POPs));
    PUTBACK;
    SvREFCNT_dec(ref);
    return (SV*)tags;
}

exifdata_t exiftool_GetValue(exiftool_t tool, const char *tagname, const char *conv) {
    dTHXa(tool);
    PERL_SET_CONTEXT(tool);
    dSP;

    SV *tag = newSVpv(tagname, 0);
    SV *ref = conv ? newSVpv(conv, 0) : newSV(0);
    PUSHMARK(SP);
    EXTEND(SP, 2 + !!conv);
    PUSHs(get_sv("exifTool", 0));
    PUSHs(tag);
    if (conv) PUSHs(ref);
    PUTBACK;

    int ok = call_method("GetValue", G_SCALAR);
    SPAGAIN;
    SV *value = ok ? POPs : newSV(0);
    if (ok) value = SvREFCNT_inc(unwrap_ref(value));
    PUTBACK;
    SvREFCNT_dec(tag);
    SvREFCNT_dec(ref);
    return value;
}

int exiftool_WriteInfo(exiftool_t tool, const char *filename) {
    dTHXa(tool);
    PERL_SET_CONTEXT(tool);
    dSP;

    SV *file = newSVpv(filename, 0);
    PUSHMARK(SP);
    EXTEND(SP, 2);
    PUSHs(get_sv("exifTool", 0));
    PUSHs(file);
    PUTBACK;

    int ok = call_method("WriteInfo", G_SCALAR);
    SPAGAIN;
    int success = ok ? (int)POPi : 0;
    PUTBACK;
    SvREFCNT_dec(file);
    return success;
}

int exiftool_SetNewValue(exiftool_t tool, const char *tagname, exifdata_t value, exifdata_t options) {
    dTHXa(tool);
    PERL_SET_CONTEXT(tool);
    dSP;

    int has_options = options && SvTYPE(options) == SVt_PVAV;
    int num_options = has_options ? (int)av_top_index(options) + 1 : 0;
    SV *tag = newSVpv(tagname, 0);
    SV *ref = wrap_ref(tool, SvREFCNT_inc(value));
    PUSHMARK(SP);
    EXTEND(SP, 3 + num_options);
    PUSHs(get_sv("exifTool", 0));
    PUSHs(tag);
    PUSHs(ref);
    for (int i = 0; i < num_options; ++i)
        PUSHs(*av_fetch((AV*)options, i, 0));
    PUTBACK;

    int ok = call_method("SetNewValue", G_SCALAR);
    SPAGAIN;
    int success = ok ? (int)POPi : 0;
    PUTBACK;
    SvREFCNT_dec(ref);
    SvREFCNT_dec(tag);
    return success;
}

exifdata_t exiftool_GetNewValue(exiftool_t tool, const char *tagname) {
    dTHXa(tool);
    PERL_SET_CONTEXT(tool);
    dSP;

    SV *tag = newSVpv(tagname, 0);
    PUSHMARK(SP);
    EXTEND(SP, 2);
    PUSHs(get_sv("exifTool", 0));
    PUSHs(tag);
    PUTBACK;

    int ok = call_method("GetNewValue", G_SCALAR);
    SPAGAIN;
    SV *value = ok ? SvREFCNT_inc(POPs) : newSV(0);
    PUTBACK;
    SvREFCNT_dec(tag);
    return value;
}

exifdata_t exiftool_SetNewValuesFromFile(exiftool_t tool, const char *filename, exifdata_t tags) {
    dTHXa(tool);
    PERL_SET_CONTEXT(tool);
    dSP;

    int has_tags = tags && SvTYPE(tags) == SVt_PVAV;
    int num_tags = has_tags ? (int)av_top_index(tags) + 1 : 0;
    SV *file = newSVpv(filename, 0);
    PUSHMARK(SP);
    EXTEND(SP, 2 + num_tags);
    PUSHs(get_sv("exifTool", 0));
    PUSHs(file);
    for (int i = 0; i < num_tags; ++i)
        PUSHs(*av_fetch((AV*)tags, i, 0));
    PUTBACK;

    int ok = call_method("SetNewValuesFromFile", G_SCALAR);
    SPAGAIN;
    SV *info = ok ? POPs : newSV(0);
    if (ok) info = SvREFCNT_inc(SvRV(info));
    PUTBACK;
    SvREFCNT_dec(file);
    return info;
}

int exiftool_CountNewValues(exiftool_t tool) {
    dTHXa(tool);
    PERL_SET_CONTEXT(tool);
    dSP;

    PUSHMARK(SP);
    XPUSHs(get_sv("exifTool", 0));
    PUTBACK;

    int ok = call_method("CountNewValues", G_SCALAR);
    SPAGAIN;
    int count = ok ? (int)POPi : 0;
    PUTBACK;
    return count;
}

int exiftool_SaveNewValues(exiftool_t tool) {
    dTHXa(tool);
    PERL_SET_CONTEXT(tool);
    dSP;

    PUSHMARK(SP);
    XPUSHs(get_sv("exifTool", 0));
    PUTBACK;

    int ok = call_method("SaveNewValues", G_SCALAR);
    SPAGAIN;
    int count = ok ? (int)POPi : 0;
    PUTBACK;
    return count;
}

void exiftool_RestoreNewValues(exiftool_t tool) {
    dTHXa(tool);
    PERL_SET_CONTEXT(tool);
    dSP;

    PUSHMARK(SP);
    XPUSHs(get_sv("exifTool", 0));
    PUTBACK;

    call_method("RestoreNewValues", G_SCALAR);
}

exifdata_t exiftool_GetTagID(exiftool_t tool, const char *tagname) {
    dTHXa(tool);
    PERL_SET_CONTEXT(tool);
    dSP;

    SV *tag = newSVpv(tagname, 0);
    PUSHMARK(SP);
    EXTEND(SP, 2);
    PUSHs(get_sv("exifTool", 0));
    PUSHs(tag);
    PUTBACK;

    int ok = call_method("GetTagID", G_SCALAR);
    SPAGAIN;
    SV *id = ok ? SvREFCNT_inc(POPs) : newSV(0);
    PUTBACK;
    SvREFCNT_dec(tag);
    return id;
}

exifdata_t exiftool_GetDescription(exiftool_t tool, const char *tagname) {
    dTHXa(tool);
    PERL_SET_CONTEXT(tool);
    dSP;

    SV *tag = newSVpv(tagname, 0);
    PUSHMARK(SP);
    EXTEND(SP, 2);
    PUSHs(get_sv("exifTool", 0));
    PUSHs(tag);
    PUTBACK;

    int ok = call_method("GetDescription", G_SCALAR);
    SPAGAIN;
    SV *desc = ok ? SvREFCNT_inc(POPs) : newSV(0);
    PUTBACK;
    SvREFCNT_dec(tag);
    return desc;
}

exifdata_t exiftool_GetGroup(exiftool_t tool, const char *tagname, const char *family) {
    dTHXa(tool);
    PERL_SET_CONTEXT(tool);
    dSP;

    SV *tag = newSVpv(tagname, 0);
    SV *ref = family ? newSVpv(family, 0) : newSV(0);
    PUSHMARK(SP);
    EXTEND(SP, 2 + !!family);
    PUSHs(get_sv("exifTool", 0));
    PUSHs(tag);
    if (family) PUSHs(ref);
    PUTBACK;

    int ok = call_method("GetGroup", G_SCALAR);
    SPAGAIN;
    SV *group = ok ? SvREFCNT_inc(POPs) : newSV(0);
    PUTBACK;
    SvREFCNT_dec(tag);
    SvREFCNT_dec(ref);
    return group;
}

exifdata_t exiftool_GetTagName(exiftool_t tool, const char *tagname) {
    dTHXa(tool);
    PERL_SET_CONTEXT(tool);
    dSP;

    SV *tag = newSVpv(tagname, 0);
    PUSHMARK(SP);
    XPUSHs(tag);
    PUTBACK;

    int ok = call_pv("GetTagName", G_SCALAR);
    SPAGAIN;
    SV *name = ok ? SvREFCNT_inc(POPs) : newSV(0);
    PUTBACK;
    SvREFCNT_dec(tag);
    return name;
}

/* === exifdata functions === */

void exifdata_Destroy(exiftool_t tool, exifdata_t data) {
    dTHXa(tool);
    SvREFCNT_dec(data);
}

exifdata_t exifdata_Copy(exiftool_t tool, exifdata_t data) {
    dTHXa(tool);
    return SvREFCNT_inc(data);
}

int exifdata_Type(exifdata_t data) {
    svtype type = SvTYPE(data);
    if (SvNIOK(data)) return 0;
    if (SvPOK(data)) return 1;
    if (SvROK(data)) return 2;
    if (type == SVt_PVAV) return 3;
    if (type == SVt_PVHV) return 4;
    return -1;
}

exifdata_t exifdata_CreateUndef(exiftool_t tool) {
    dTHXa(tool);
    return newSV(0);
}

exifdata_t exifdata_CreateNumber(exiftool_t tool, double num) {
    dTHXa(tool);
    return newSVnv(num);
}

double exifdata_Number(exiftool_t tool, exifdata_t data) {
    dTHXa(tool);
    if (!SvNIOK(data)) return 0.0;
    return SvNV(data);
}

exifdata_t exifdata_CreateString(exiftool_t tool, const char *str) {
    dTHXa(tool);
    return newSVpv(str, 0);
}

const char *exifdata_String(exiftool_t tool, exifdata_t data) {
    dTHXa(tool);
    size_t length = 0;
    if (!SvPOK(data) && !SvNIOK(data)) return "";
    return SvPV(data, length);
}

exifdata_t exifdata_CreateBuffer(exiftool_t tool, const char *buf, size_t len) {
    dTHXa(tool);
    SV *value = newSVpv(buf, len);
    return newRV_noinc(value);
}

const char *exifdata_Buffer(exiftool_t tool, exifdata_t data, size_t *len) {
    dTHXa(tool);
    if (!SvROK(data)) return *len = 0, "";
    return SvPV(SvRV(data), *len);
}

exifdata_t exifdata_CreateList(exiftool_t tool) {
    dTHXa(tool);
    return (SV*)newAV();
}

int exifdata_Length(exiftool_t tool, exifdata_t data) {
    dTHXa(tool);
    if (SvTYPE(data) != SVt_PVAV) return 0;
    return (int)av_top_index(data) + 1;
}

exifdata_t exifdata_Item(exiftool_t tool, exifdata_t data, int idx) {
    dTHXa(tool);
    if (SvTYPE(data) != SVt_PVAV) return &PL_sv_undef;
    SV **item = av_fetch((AV*)data, idx, 0);
    return item ? unwrap_ref(*item) : &PL_sv_undef;
}

void exifdata_Append(exiftool_t tool, exifdata_t data, exifdata_t item) {
    dTHXa(tool);
    if (SvTYPE(data) != SVt_PVAV) return;
    av_push((AV*)data, wrap_ref(tool, item));
}

exifdata_t exifdata_CreateHash(exiftool_t tool) {
    dTHXa(tool);
    return (SV*)newHV();
}

void exifdata_RewindKeys(exiftool_t tool, exifdata_t data) {
    dTHXa(tool);
    if (SvTYPE(data) != SVt_PVHV) return;
    hv_iterinit((HV*)data);
}

const char *exifdata_NextKey(exiftool_t tool, exifdata_t data) {
    dTHXa(tool);
    int length = 0;
    if (SvTYPE(data) != SVt_PVHV) return NULL;
    HE *entry = hv_iternext((HV*)data);
    return entry ? hv_iterkey(entry, &length) : NULL;
}

exifdata_t exifdata_Value(exiftool_t tool, exifdata_t data, const char *key) {
    dTHXa(tool);
    if (SvTYPE(data) != SVt_PVHV) return &PL_sv_undef;
    SV **value = hv_fetch((HV*)data, key, (int)strlen(key), 0);
    return value ? unwrap_ref(*value) : &PL_sv_undef;
}

void exifdata_Set(exiftool_t tool, exifdata_t data, const char *key, exifdata_t value) {
    dTHXa(tool);
    if (SvTYPE(data) != SVt_PVHV) return;
    hv_store((HV*)data, key, (int)strlen(key), wrap_ref(tool, value), 0);
}
