#include <windows.h>
#include <winspool.h>
#include <stdio.h>

#include <dmtx.h>

/* Copied from dmtxwrite. */
static int
dump_ascii(DmtxEncode *enc)
{
    int symbolRow, symbolCol;

    fputc('\n', stdout);

    /* ASCII prints from top to bottom */
    for (symbolRow = enc->region.symbolRows - 1; symbolRow >= 0; symbolRow--)
    {
        fputs("    ", stdout);
        for (symbolCol = 0; symbolCol < enc->region.symbolCols; symbolCol++)
        {
            fputs((dmtxSymbolModuleStatus(
                       enc->message,
                       enc->region.sizeIdx,
                       symbolRow,
                       symbolCol) &
                   DmtxModuleOnRGB)
                      ? "XX"
                      : "  ",
                  stdout);
        }
        fputs("\n", stdout);
    }

    fputc('\n', stdout);

    return 0;
}

int make_datamatrix(const char *data)
{
    int rc;
    DmtxEncode *enc;

    enc = dmtxEncodeCreate();
    if (enc == NULL)
    {
        printf("Failed to create encoder\n");
        rc = -EINVAL;
        goto exit;
    }

    /* Set output image properties */
    dmtxEncodeSetProp(enc, DmtxPropPixelPacking, DmtxPack24bppRGB);
    dmtxEncodeSetProp(enc, DmtxPropImageFlip, DmtxFlipNone);
    dmtxEncodeSetProp(enc, DmtxPropRowPadBytes, 0);

    /* Set encoding options */
    dmtxEncodeSetProp(enc, DmtxPropMarginSize, 10);
    dmtxEncodeSetProp(enc, DmtxPropModuleSize, 5);
    dmtxEncodeSetProp(enc, DmtxPropScheme, DmtxSchemeAutoBest);
    dmtxEncodeSetProp(enc, DmtxPropSizeRequest, DmtxSymbolSquareAuto);

    rc = dmtxEncodeDataMatrix(enc, strlen(data), (unsigned char *)data);
    if (rc == DmtxFail)
    {
        printf("Failed to encode data\n");
        rc = -EINVAL;
        goto exit;
    }

    dump_ascii(enc);

exit:
    if (enc != NULL)
        dmtxEncodeDestroy(&enc);
}

int main(int argc, char **argv)
{
    int rc;
    const char *printer_name = NULL;

    const char *sample_data = "0123456789abcde";

    if (argc != 2)
    {
        printf("Usage: %s <printer name>\n", argv[0]);
        return -EINVAL;
    }

    printer_name = argv[1];

    printf("Generating datamatrix for: %s\n", sample_data);
    rc = make_datamatrix(sample_data);
    if (rc < 0)
    {
        printf("Failed to generate datamatrix\n");
    }

    fflush(stdout);
    fflush(stderr);

    return 0;
}
