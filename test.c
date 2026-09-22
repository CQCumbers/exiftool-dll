#include "exiftool.h"
#include <stdio.h>

/* defined in p3dci.obj by bin2obj */
#ifndef __linux__
extern int _p3dci_icc_size;
extern char _p3dci_icc[];
#else
//The size field from objcopy causes a segfault if accessed, so we need to use the start and end instead
//Some googling indicates this is likely from compiling with -fPIC
extern char _binary_p3dci_icc_start[];
extern char _binary_p3dci_icc_end[];
#endif

int main(int argc, char *argv[]) {
    if (argc < 3) return puts("Usage: ./test <readfile> <writefile>"), 0;
    exiftool_t tool = exiftool_Create();
    int ok = exiftool_ExtractInfo(tool, argv[1], NULL);
    printf("ExtractInfo return %d\n", ok);

    exifdata_t options = exifdata_CreateList(tool);
    exifdata_Append(tool, options, exifdata_CreateString(tool, "Duplicates"));
    exifdata_Append(tool, options, exifdata_CreateNumber(tool, 0));
    exifdata_Append(tool, options, exifdata_CreateString(tool, "Sort"));
    exifdata_Append(tool, options, exifdata_CreateString(tool, "File"));
    exifdata_Append(tool, options, exifdata_CreateString(tool, "Unknown"));
    exifdata_Append(tool, options, exifdata_CreateNumber(tool, 1));

    exifdata_t oldOpt = exiftool_Options(tool, options);
    printf("Old Unknown : %f\n", exifdata_Number(tool, oldOpt));
    exifdata_Destroy(tool, options);
    exifdata_Destroy(tool, oldOpt);

    exifdata_t info = exiftool_GetInfo(tool, NULL);
    exifdata_t foundTags = exiftool_GetTagList(tool, NULL);

    for (int i = 0; i < exifdata_Length(tool, foundTags); ++i) {
        exifdata_t key = exifdata_Item(tool, foundTags, i);
        const char *keyname = exifdata_String(tool, key);
        exifdata_t val = exifdata_Value(tool, info, keyname);
        if (exifdata_Type(val) == EXIFDATA_BUFFER) {
            size_t length = 0, length2 = 0;
            exifdata_Buffer(tool, val, &length);
            printf("Found %s : (Binary data %zu)\n", keyname, length);

            val = exiftool_GetValue(tool, keyname, "ValueConv");
            exifdata_Buffer(tool, val, &length2);
            exifdata_Destroy(tool, val);
            if (length != length2) printf("Binary GetValue failed\n");
        } else {
            const char *valname = exifdata_String(tool, val);
            printf("Found %s : %s\n", keyname, valname);
        }
    }

    const char *keyname = NULL;
    exifdata_RewindKeys(tool, info);
    printf("Iterating over keys: ");
    while ((keyname = exifdata_NextKey(tool, info))) printf("%s ", keyname);
    printf("\n\n");

    const char tagname[] = "ImageWidth";
    exifdata_t val = exiftool_GetValue(tool, tagname, "PrintConv");
    exifdata_t id = exiftool_GetTagID(tool, tagname);
    exifdata_t name = exiftool_GetTagName(tool, tagname);
    exifdata_t desc = exiftool_GetDescription(tool, tagname);
    exifdata_t group = exiftool_GetGroup(tool, tagname, "0:1");

    printf("%s %s::%s\n", exifdata_String(tool, id),
        exifdata_String(tool, group), exifdata_String(tool, name));
    printf("Description: %s\n", exifdata_String(tool, desc));
    printf("Value: %s\n", exifdata_String(tool, val));

    exifdata_Destroy(tool, group);
    exifdata_Destroy(tool, desc);
    exifdata_Destroy(tool, name);
    exifdata_Destroy(tool, id);
    exifdata_Destroy(tool, val);

    options = exifdata_CreateList(tool);
    exifdata_Append(tool, options, exifdata_CreateString(tool, "Protected"));
    exifdata_Append(tool, options, exifdata_CreateNumber(tool, 1));

    exifdata_t rules = exifdata_CreateList(tool);
    exifdata_Append(tool, rules, exifdata_CreateString(tool,
        "Comment<ISO=$ISO Aperture=$aperture Exposure=$shutterSpeed"));
    exifdata_Append(tool, rules, options);
    exifdata_t values = exiftool_SetNewValuesFromFile(tool, argv[1], rules);

    const char *warning = exifdata_String(tool, exifdata_Value(tool, values, "Warning"));
    printf("SetNewValuesFromFile Warning : %s\n", warning);
    exifdata_Destroy(tool, values);

    printf("Set %d new values\n", exiftool_CountNewValues(tool));
    val = exiftool_GetNewValue(tool, "Comment");
    printf("New Comment : %s\n", exifdata_String(tool, val));
    exifdata_Destroy(tool, val);

#ifndef __linux__
    val = exifdata_CreateBuffer(tool, _p3dci_icc, _p3dci_icc_size);
#else
    val = exifdata_CreateBuffer(tool, _binary_p3dci_icc_start, (_binary_p3dci_icc_end - _binary_p3dci_icc_start));
#endif
    int status = exiftool_SetNewValue(tool, "ICC_Profile", val, options);
    printf("SetNewValue returned %d\n", status);
    exifdata_Destroy(tool, val);

    int saved = exiftool_SaveNewValues(tool);
    printf("Saved %d times\n", saved);

    val = exifdata_CreateString(tool, "Adobe Standard");
    status = exiftool_SetNewValue(tool, "EXIF:ProfileName", val, options);
    printf("SetNewValue returned %d\n", status);
    exifdata_Destroy(tool, val);
    exifdata_Destroy(tool, rules);

    printf("Set %d new values\n", exiftool_CountNewValues(tool));
    val = exiftool_GetNewValue(tool, "EXIF:ProfileName");
    printf("New ProfileName : %s\n", exifdata_String(tool, val));
    exifdata_Destroy(tool, val);

    exiftool_RestoreNewValues(tool);
    printf("Restored %d new values\n", exiftool_CountNewValues(tool));
    status = exiftool_WriteInfo(tool, argv[2]);
    printf("WriteInfo returned %d\n", status);

    exifdata_Destroy(tool, foundTags);
    exifdata_Destroy(tool, info);
    exiftool_Destroy(tool);
    return 0;
}
