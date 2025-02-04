#include "windows.h"
#include "winspool.h"
#include "errno.h"
#include "stdio.h"

void list_print_processor_datatypes(LPCSTR processor)
{
    DWORD needed = 0, returned = 0;
    DATATYPES_INFO_1 *pInfo = NULL;

    EnumPrintProcessorDatatypes(NULL, (LPSTR)processor, 1, NULL, 0, &needed, &returned);
    if (needed <= 0)
    {
        printf("    Failed to get print processor data types\n");
        goto exit;
    }

    pInfo = (DATATYPES_INFO_1 *)malloc(needed);
    if (pInfo == NULL)
    {
        printf("    Failed to allocate memory\n");
        goto exit;
    }

    if (!EnumPrintProcessorDatatypes(NULL, (LPSTR)processor, 1, (LPBYTE)pInfo, needed, &needed, &returned))
    {
        printf("    Failed to get print processor data types\n");
        goto exit;
    }

    for (DWORD i = 0; i < returned; i++)
    {
        DATATYPES_INFO_1 *pDatatypeInfo = &pInfo[i];
        wprintf(L"    %s\n", pDatatypeInfo->pName);
    }

exit:
    if (pInfo != NULL)
        free(pInfo);
}

void list_capabilities(LPCSTR name)
{
    DWORD pageCount = 0;
    WORD *pSizes = NULL;
    POINT *pDimensions = NULL;
    LPSTR pPageNames = NULL;
    DWORD colourCapability = 0;

    pageCount = DeviceCapabilities(name, NULL, DC_PAPERS, NULL, NULL);
    if (pageCount <= 0)
    {
        printf("    Failed to get page sizes\n");
        goto exit;
    }

    pSizes = (WORD *)malloc(pageCount * sizeof(WORD));
    if (pSizes == NULL)
    {
        printf("    Failed to allocate memory\n");
        goto exit;
    }

    pDimensions = (POINT *)malloc(pageCount * sizeof(POINT));
    if (pDimensions == NULL)
    {
        printf("    Failed to allocate memory\n");
        goto exit;
    }

    pPageNames = (LPSTR)malloc(pageCount * 64);
    if (pPageNames == NULL)
    {
        printf("    Failed to allocate memory\n");
        goto exit;
    }

    if (DeviceCapabilities(name, NULL, DC_PAPERS, (LPSTR)pSizes, NULL) <= 0)
    {
        printf("    Failed to get page sizes\n");
        goto exit;
    }

    if (DeviceCapabilities(name, NULL, DC_PAPERSIZE, (LPSTR)pDimensions, NULL) <= 0)
    {
        printf("    Failed to get page dimensions\n");
        goto exit;
    }

    if (DeviceCapabilities(name, NULL, DC_PAPERNAMES, pPageNames, NULL) <= 0)
    {
        printf("    Failed to get page names\n");
        goto exit;
    }

    printf("  Found %u page types\n", pageCount);

    for (DWORD i = 0; i < pageCount; i++)
    {
        WORD size = pSizes[i];
        POINT *pDimension = &pDimensions[i];
        LPSTR pPageName = pPageNames + (i * 64);
        printf("    %u: %s %ux%u\n", size, pPageName, pDimension->x, pDimension->y);
    }

    colourCapability = DeviceCapabilities(name, NULL, DC_COLORDEVICE, NULL, NULL);
    if (colourCapability == 1)
    {
        printf("  Colour: Yes\n");
    }
    else
    {
        printf("  Colour: No\n");
    }

exit:
    if (pSizes != NULL)
        free(pSizes);

    if (pDimensions != NULL)
        free(pDimensions);

    if (pPageNames != NULL)
        free(pPageNames);
}

void get_printer_dpi(LPCSTR name)
{
    HDC hPrinter = NULL;
    DWORD xDPI = 0, yDPI = 0;

    hPrinter = CreateDC("WINSPOOL", name, NULL, NULL);

    xDPI = GetDeviceCaps(hPrinter, LOGPIXELSX);
    yDPI = GetDeviceCaps(hPrinter, LOGPIXELSY);

    DeleteDC(hPrinter);

    printf("  DPI: %ux%u\n", xDPI, yDPI);
}

void list_printers(void)
{
    DWORD needed = 0, returned = 0;
    PRINTER_INFO_2 *pInfo = NULL;

    printf("Listing printers\n");

    EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 2, NULL, 0, &needed, &returned);
    if (needed <= 0)
    {
        printf("Failed to get printer info\n");
        goto exit;
    }

    pInfo = (PRINTER_INFO_2 *)malloc(needed);
    if (pInfo == NULL)
    {
        printf("Failed to allocate memory\n");
        goto exit;
    }

    if (!EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 2, (LPBYTE)pInfo, needed, &needed, &returned))
    {
        printf("Failed to get printer info\n");
        goto exit;
    }

    printf("Found %u printers\n", returned);
    for (DWORD i = 0; i < returned; i++)
    {
        PRINTER_INFO_2 *pPrinterInfo = &pInfo[i];

        printf("%s\n", pPrinterInfo->pPrinterName);
        printf("  port: %s\n", pPrinterInfo->pPortName);
        printf("  driver: %s\n", pPrinterInfo->pDriverName);
        printf("  processor: %s\n", pPrinterInfo->pPrintProcessor);
        list_print_processor_datatypes(pPrinterInfo->pPrintProcessor);
        list_capabilities(pPrinterInfo->pPrinterName);
        get_printer_dpi(pPrinterInfo->pPrinterName);
    }

exit:
    if (pInfo != NULL)
        free(pInfo);
}

int main()
{
    list_printers();

    return 0;
}
