#include "windows.h"
#include "winspool.h"
#include "errno.h"
#include "stdio.h"

static const char *A4_PAGE_NAME = "A4";

struct page_details
{
    short size;
    POINT dimensions;
    char name[64];
};

void draw(HDC canvas)
{
    Rectangle(canvas, 10, 10, 100, 100);
}

struct page_details *get_page_details(const char *printer_name, const char *page_name)
{
    int rc = 0;
    int count = 0;
    short *sizes = NULL;
    POINT *dimensions = NULL;
    char *page_names = NULL;

    struct page_details *details = NULL;

    count = DeviceCapabilities(printer_name, NULL, DC_PAPERS, NULL, NULL);
    if (count <= 0)
    {
        fprintf(stderr, "Failed to get page sizes\n");
        rc = -EINVAL;
        goto exit;
    }

    sizes = (short *)malloc(count * sizeof(short));
    if (sizes == NULL)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        rc = -ENOMEM;
        goto exit;
    }

    if (DeviceCapabilities(printer_name, NULL, DC_PAPERS, (char *)sizes, NULL) <= 0)
    {
        fprintf(stderr, "Failed to get page sizes\n");
        rc = -EINVAL;
        goto exit;
    }

    dimensions = (POINT *)malloc(count * sizeof(POINT));
    if (dimensions == NULL)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        rc = -ENOMEM;
        goto exit;
    }

    if (DeviceCapabilities(printer_name, NULL, DC_PAPERSIZE, (char *)dimensions, NULL) <= 0)
    {
        fprintf(stderr, "Failed to get page dimensions\n");
        rc = -EINVAL;
        goto exit;
    }

    page_names = (char *)malloc(count * 64);
    if (page_names == NULL)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        rc = -ENOMEM;
        goto exit;
    }

    if (DeviceCapabilities(printer_name, NULL, DC_PAPERNAMES, page_names, NULL) <= 0)
    {
        fprintf(stderr, "Failed to get page names\n");
        rc = -EINVAL;
        goto exit;
    }

    for (int i = 0; i < count; i++)
    {
        const char *name = page_names + (i * 64);
        if (strcmp(name, page_name) == 0)
        {
            details = (struct page_details *)malloc(sizeof(struct page_details));
            if (details == NULL)
            {
                fprintf(stderr, "Failed to allocate memory\n");
                rc = -ENOMEM;
                goto exit;
            }

            details->size = sizes[i];
            details->dimensions = dimensions[i];
            strncpy(details->name, name, sizeof(details->name));
            break;
        }
    }

exit:
    if (sizes != NULL)
        free(sizes);

    if (dimensions != NULL)
        free(dimensions);

    if (page_names != NULL)
        free(page_names);

    if (rc < 0)
        errno = rc;

    return details;
}

int demo_print(const char *printer_name, const char *page_size)
{
    int rc;
    struct page_details *details = NULL;
    HDC canvas = NULL;
    int xres, yres;
    int logicalx, logicaly;
    HANDLE hPrinter = NULL;
    HDC printer = NULL;
    int devmode_size = 0;
    DEVMODEA *devmode;
    HENHMETAFILE emf = NULL;
    DOCINFOA doc_info;
    RECT rect;

    details = get_page_details(printer_name, page_size);
    if (details == NULL)
    {
        fprintf(stderr, "Failed to get page details\n");
        rc = -EINVAL;
        goto exit;
    }

    canvas = CreateEnhMetaFile(NULL, NULL, NULL, NULL);
    if (canvas == NULL)
    {
        fprintf(stderr, "Failed to create metafile\n");
        rc = -EINVAL;
        goto exit;
    }

    /* TODO: This is copied from ChatGPT and isn't working. I suspect the
     * coordinate system is wrong here, or on the printer down below. Not
     * sure why we need two coordinate spaces like this! */

    xres = GetDeviceCaps(canvas, LOGPIXELSX);
    yres = GetDeviceCaps(canvas, LOGPIXELSY);
    if (xres <= 0 || yres <= 0)
    {
        fprintf(stderr, "Failed to get DPI\n");
        rc = -EINVAL;
        goto exit;
    }

    if (SetMapMode(canvas, MM_ANISOTROPIC) == 0)
    {
        fprintf(stderr, "Failed to set map mode\n");
        rc = -EINVAL;
        goto exit;
    }

    if (SetWindowExtEx(canvas, details->dimensions.x / 10, details->dimensions.y / 10, NULL) == 0)
    {
        fprintf(stderr, "Failed to set window extents\n");
        rc = -EINVAL;
        goto exit;
    }

    logicalx = xres * details->dimensions.x / 10 / 25.4;
    logicaly = yres * details->dimensions.y / 10 / 25.4;

    if (SetViewportExtEx(canvas, xres, yres, NULL) == 0)
    {
        fprintf(stderr, "Failed to set viewport extents\n");
        rc = -EINVAL;
        goto exit;
    }

    draw(canvas);

    emf = CloseEnhMetaFile(canvas);
    if (emf == NULL)
    {
        fprintf(stderr, "Failed to close metafile\n");
        rc = -EINVAL;
        goto exit;
    }

    if (OpenPrinter((char *)printer_name, &hPrinter, NULL) == 0)
    {
        fprintf(stderr, "Failed to open printer\n");
        rc = -EINVAL;
        goto exit;
    }

    devmode_size = DocumentProperties(NULL, hPrinter, (char *)printer_name, NULL, NULL, 0);
    if (devmode_size <= 0)
    {
        fprintf(stderr, "Failed to get printer properties size\n");
        rc = -EINVAL;
        goto exit;
    }

    devmode = (DEVMODEA *)malloc(devmode_size);
    if (devmode == NULL)
    {
        fprintf(stderr, "Failed to allocate %u octets\n", devmode_size);
        rc = -ENOMEM;
        goto exit;
    }

    if (DocumentProperties(NULL, hPrinter, (char *)printer_name, devmode, NULL, DM_OUT_BUFFER) != IDOK)
    {
        fprintf(stderr, "Failed to get printer properties\n");
        rc = -EINVAL;
        goto exit;
    }

    devmode->dmPaperSize = details->size;

    if (DocumentProperties(NULL, hPrinter, (char *)printer_name, devmode, devmode, DM_IN_BUFFER | DM_OUT_BUFFER) != IDOK)
    {
        fprintf(stderr, "Failed to set printer properties\n");
        rc = -EINVAL;
        goto exit;
    }

    printer = CreateDC("WINSPOOL", printer_name, NULL, devmode);
    if (printer == NULL)
    {
        fprintf(stderr, "Failed to create printer with devmode\n");
        rc = -EINVAL;
        goto exit;
    }

    printer = ResetDC(printer, devmode);
    if (printer == NULL)
    {
        fprintf(stderr, "Failed to reset printer\n");
        rc = -EINVAL;
        goto exit;
    }

    memset(&doc_info, 0, sizeof(doc_info));
    doc_info.cbSize = sizeof(doc_info);
    doc_info.lpszDocName = "Demo Print";

    if (StartDocA(printer, &doc_info) <= 0)
    {
        fprintf(stderr, "Failed to start document\n");
        rc = -EINVAL;
        goto exit;
    }

    if (StartPage(printer) <= 0)
    {
        fprintf(stderr, "Failed to start page\n");
        EndDoc(printer);
        rc = -EINVAL;
        goto exit;
    }

    rect.left = 0;
    rect.top = 0;
    rect.right = GetDeviceCaps(printer, HORZRES);
    rect.bottom = GetDeviceCaps(printer, VERTRES);

    if (PlayEnhMetaFile(printer, emf, &rect) == 0)
    {
        fprintf(stderr, "Failed to play metafile\n");
        rc = -EINVAL;
    }

    if (EndPage(printer) <= 0)
    {
        fprintf(stderr, "Failed to end page\n");
        rc = -EINVAL;
        goto exit;
    }

    if (EndDoc(printer) <= 0)
    {
        fprintf(stderr, "Failed to end document\n");
        rc = -EINVAL;
        goto exit;
    }

    rc = 0;

exit:
    if (details != NULL)
        free(details);

    if (emf != NULL)
        DeleteEnhMetaFile(emf);

    if (hPrinter != NULL)
        ClosePrinter(hPrinter);

    if (printer != NULL)
        DeleteDC(printer);

    return rc;
}

int main(int argc, char **argv)
{
    const char *printer_name = NULL;

    if (argc != 2)
    {
        printf("Usage: %s <printer name>\n", argv[0]);
        return -EINVAL;
    }

    printer_name = argv[1];

    printf("Printing to: %s\n", printer_name);
    demo_print(printer_name, A4_PAGE_NAME);

    return 0;
}
