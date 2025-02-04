#include "windows.h"
#include "winspool.h"
#include "errno.h"
#include "stdio.h"

void list_print_processor_datatypes(LPCSTR processor)
{
    DWORD needed = 0;
    DWORD returned = 0;
    DATATYPES_INFO_1 *pInfo = NULL;

    printf("  Listing print processor data types\n");

    EnumPrintProcessorDatatypes(NULL, (LPSTR)processor, 1, NULL, 0, &needed, &returned);
    if (needed <= 0)
    {
        printf("  Failed to get print processor data types\n");
        goto exit;
    }

    pInfo = (DATATYPES_INFO_1 *)malloc(needed);
    if (pInfo == NULL)
    {
        printf("  Failed to allocate memory\n");
        goto exit;
    }

    if (!EnumPrintProcessorDatatypes(NULL, (LPSTR)processor, 1, (LPBYTE)pInfo, needed, &needed, &returned))
    {
        printf("  Failed to get print processor data types\n");
        goto exit;
    }

    printf("  Found %u data type/s\n", returned);
    for (DWORD i = 0; i < returned; i++)
    {
        DATATYPES_INFO_1 *pDatatypeInfo = &pInfo[i];
        wprintf(L"    %s\n", pDatatypeInfo->pName);
    }

exit:
    if (pInfo != NULL)
        free(pInfo);
}

void list_print_processors()
{
    DWORD needed = 0;
    DWORD returned = 0;
    PRINTPROCESSOR_INFO_1 *pInfo = NULL;

    printf("Listing print processors\n");

    EnumPrintProcessors(NULL, NULL, 1, NULL, 0, &needed, &returned);
    if (needed <= 0)
    {
        printf("Failed to get print processor info\n");
        goto exit;
    }

    pInfo = (PRINTPROCESSOR_INFO_1 *)malloc(needed);
    if (pInfo == NULL)
    {
        printf("Failed to allocate memory\n");
        goto exit;
    }

    if (!EnumPrintProcessors(NULL, NULL, 1, (LPBYTE)pInfo, needed, &needed, &returned))
    {
        printf("Failed to get print processor info\n");
        goto exit;
    }

    printf("Found %u print processor/s\n", returned);
    for (DWORD i = 0; i < returned; i++)
    {
        PRINTPROCESSOR_INFO_1 *pProcessorInfo = &pInfo[i];

        wprintf(L"%s\n", pProcessorInfo->pName);
        list_print_processor_datatypes(pProcessorInfo->pName);
    }

exit:
    if (pInfo != NULL)
        free(pInfo);
}

void list_printers(void)
{
    DWORD needed = 0;
    DWORD returned = 0;
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

        printf("%s\n", pInfo->pPrinterName);
        printf("  port: %s\n", pInfo->pPortName);
        printf("  driver: %s\n", pInfo->pDriverName);
        printf("  processor: %s\n", pInfo->pPrintProcessor);
        printf("  data type: %s\n", pInfo->pDatatype);
    }

exit:
    if (pInfo != NULL)
        free(pInfo);
}

int main()
{
    list_printers();
    list_print_processors();

    return 0;
}
