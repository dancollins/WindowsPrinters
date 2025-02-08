#include "windows.h"
#include "winspool.h"
#include "errno.h"
#include "stdio.h"

static const char *A4_PAGE_NAME = "A4";
static const char *EMF_FILE_NAME = "demo_print.emf";

struct page_details
{
    short size;
    POINT dimensions;
    char name[64];
};

struct coordinate_space
{
    struct
    {
        int width;
        int height;
        int offset_x;
        int offset_y;
    } logical;

    struct
    {
        int width;
        int height;
        int offset_x;
        int offset_y;
    } device;
};

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
        printf("Failed to get page sizes\n");
        rc = -EINVAL;
        goto exit;
    }

    sizes = (short *)malloc(count * sizeof(short));
    if (sizes == NULL)
    {
        printf("Failed to allocate memory\n");
        rc = -ENOMEM;
        goto exit;
    }

    if (DeviceCapabilities(printer_name, NULL, DC_PAPERS, (char *)sizes, NULL) <= 0)
    {
        printf("Failed to get page sizes\n");
        rc = -EINVAL;
        goto exit;
    }

    dimensions = (POINT *)malloc(count * sizeof(POINT));
    if (dimensions == NULL)
    {
        printf("Failed to allocate memory\n");
        rc = -ENOMEM;
        goto exit;
    }

    if (DeviceCapabilities(printer_name, NULL, DC_PAPERSIZE, (char *)dimensions, NULL) <= 0)
    {
        printf("Failed to get page dimensions\n");
        rc = -EINVAL;
        goto exit;
    }

    page_names = (char *)malloc(count * 64);
    if (page_names == NULL)
    {
        printf("Failed to allocate memory\n");
        rc = -ENOMEM;
        goto exit;
    }

    if (DeviceCapabilities(printer_name, NULL, DC_PAPERNAMES, page_names, NULL) <= 0)
    {
        printf("Failed to get page names\n");
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
                printf("Failed to allocate memory\n");
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

int set_page_size(
    const char *printer_name,
    const struct page_details *details,
    DEVMODE **devmode)
{
    int rc = 0;
    HANDLE printer = NULL;
    int devmode_size = 0;

    *devmode = NULL;

    printf(
        "Setting page size on \"%s\" to: \"%s\"\n",
        printer_name,
        details->name);

    if (OpenPrinter((char *)printer_name, &printer, NULL) == 0)
    {
        printf("Failed to open printer\n");
        rc = -EINVAL;
        goto exit;
    }

    devmode_size = DocumentProperties(
        NULL, printer, (char *)printer_name, NULL, NULL, 0);

    if (devmode_size <= 0)
    {
        printf("Failed to get printer properties size\n");
        rc = -EINVAL;
        goto exit;
    }

    *devmode = (DEVMODE *)malloc(devmode_size);
    if (*devmode == NULL)
    {
        printf("Failed to allocate %u octets\n", devmode_size);
        rc = -ENOMEM;
        goto exit;
    }

    if (DocumentProperties(
            NULL,
            printer,
            (char *)printer_name,
            *devmode,
            NULL,
            DM_OUT_BUFFER) != IDOK)
    {
        printf("Failed to get printer properties\n");
        rc = -EINVAL;
        goto exit;
    }

    (*devmode)->dmPaperSize = details->size;
    (*devmode)->dmOrientation = DMORIENT_PORTRAIT;

    if (DocumentProperties(
            NULL,
            printer,
            (char *)printer_name,
            *devmode,
            *devmode,
            DM_IN_BUFFER | DM_OUT_BUFFER) != IDOK)
    {
        printf("Failed to set printer properties\n");
        rc = -EINVAL;
        goto exit;
    }

    if (ClosePrinter(printer) == 0)
    {
        printf("Failed to close printer\n");
        rc = -EINVAL;
        goto exit;
    }

    return 0;

exit:
    if (printer != NULL)
        ClosePrinter(printer);

    if (*devmode == NULL)
        free(*devmode);

    return rc;
}

void draw(HDC printer, const struct coordinate_space *space)
{
    POINT p[2] = {{100, 100}, {1100, 1100}};

    printf("  Rectangle (1/10 mm) (%d,%d),(%d,%d)\n", p[0].x, p[0].y, p[1].x, p[1].y);
    Rectangle(printer, 100, 100, 1100, 1100);

    LPtoDP(printer, p, 2);
    printf("  Rectangle (pixels) (%d,%d),(%d,%d)\n", p[0].x, p[0].y, p[1].x, p[1].y);
}

int demo_print(const char *printer_name, const char *page_size)
{
    int rc;
    struct page_details *details = NULL;
    DEVMODE *devmode = NULL;
    struct coordinate_space space;
    HDC printer = NULL;
    DOCINFOA doc_info = {0};

    /* Configure the printer - for now all we're doing is setting the
     * page size. We have to do this first, because we then ask the printer
     * to tell us, based on this page size, how many pixels it has in X
     * and Y. */
    details = get_page_details(printer_name, page_size);
    if (details == NULL)
    {
        printf("Failed to get page details\n");
        rc = -EINVAL;
        goto exit;
    }

    rc = set_page_size(printer_name, details, &devmode);
    if (rc < 0)
    {
        printf("Failed to set page size\n");
        goto exit;
    }

    printer = CreateDC("WINSPOOL", printer_name, NULL, devmode);
    if (printer == NULL)
    {
        printf("Failed to create printer\n");
        rc = -EINVAL;
        goto exit;
    }

    /* The printer coordinate space uses pixels, at some DPI. Our EMF
     * represents an entire page - but we also need to account for the
     * actual printable area. We handle this by capturing the offsets. */
    space.device.width = GetDeviceCaps(printer, PHYSICALWIDTH);
    space.device.height = GetDeviceCaps(printer, PHYSICALHEIGHT);
    space.device.offset_x = GetDeviceCaps(printer, PHYSICALOFFSETX);
    space.device.offset_y = GetDeviceCaps(printer, PHYSICALOFFSETY);

    int printer_dpi_x = GetDeviceCaps(printer, LOGPIXELSX);
    int printer_dpi_y = GetDeviceCaps(printer, LOGPIXELSY);
    int printer_horz_res = GetDeviceCaps(printer, HORZRES);
    int printer_vert_res = GetDeviceCaps(printer, VERTRES);

    printf(
        "Printer resolution: %d x %d DPI\n",
        printer_dpi_x,
        printer_dpi_y);

    printf(
        "Physical page size: %d x %d px\n",
        space.device.width,
        space.device.height);

    printf(
        "Printable page size: %d x %d px\n",
        printer_horz_res,
        printer_vert_res);

    printf(
        "Print offsets (X, Y): (%d, %d) px\n",
        space.device.offset_x,
        space.device.offset_y);

    /* Our drawing is done in logical units and, for convenience, we'll use
     * 1/10 mm units (as returned by DC_PAPERSIZE) and represent the entire
     * page. */
    space.logical.width = details->dimensions.x;
    space.logical.height = details->dimensions.y;

    double scale_x = (double)space.logical.width / (double)space.device.width;
    double scale_y = (double)space.logical.height / (double)space.device.height;

    space.logical.offset_x = (int)(space.device.offset_x * scale_x);
    space.logical.offset_y = (int)(space.device.offset_y * scale_y);

    printf("Logical page size: %d x %d px\n", space.logical.width, space.logical.height);
    printf("Logical offsets (X, Y): (%d, %d) px\n", space.logical.offset_x, space.logical.offset_y);
    printf("Logical scaling factor: %f x %f\n", scale_x, scale_y);

    /* Start the print job! */
    doc_info.cbSize = sizeof(doc_info);
    doc_info.lpszDocName = "DEMO_PRINT";

    printf("Starting print job\n");

    if (StartDoc(printer, &doc_info) <= 0)
    {
        printf("Failed to start document\n");
        rc = -EINVAL;
        goto exit;
    }

    if (StartPage(printer) <= 0)
    {
        printf("Failed to start page\n");
        rc = -EINVAL;
        goto exit;
    }

    if (SaveDC(printer) == 0)
    {
        printf("Failed to save DC\n");
        rc = -EINVAL;
        goto exit;
    }

    /* We want to ensure symmetric scaling. */
    if (SetMapMode(printer, MM_ISOTROPIC) == 0)
    {
        printf("Failed to set map mode\n");
        rc = -EINVAL;
        goto exit;
    }

    /* Set up logical and device coordinate systems. */
    if (SetWindowExtEx(printer, space.logical.width, space.logical.height, NULL) == 0)
    {
        printf("Failed to set window extents\n");
        rc = -EINVAL;
        goto exit;
    }

    if (SetViewportExtEx(printer, space.device.width, space.device.height, NULL) == 0)
    {
        printf("Failed to set viewport extents\n");
        rc = -EINVAL;
        goto exit;
    }

    if (SetViewportOrgEx(printer, 0, 0, NULL) == 0)
    {
        printf("Failed to set viewport origin\n");
        rc = -EINVAL;
        goto exit;
    }

    /* Draw what we want to print. */
    draw(printer, &space);

    if (RestoreDC(printer, -1) == 0)
    {
        printf("Failed to restore DC\n");
        rc = -EINVAL;
        goto exit;
    }

    if (EndPage(printer) <= 0)
    {
        printf("Failed to end page\n");
        rc = -EINVAL;
        goto exit;
    }

    if (EndDoc(printer) <= 0)
    {
        printf("Failed to end document\n");
        rc = -EINVAL;
        goto exit;
    }

    printf("Print job complete!\n");

    rc = 0;

exit:
    if (details != NULL)
        free(details);

    if (printer != NULL)
        DeleteDC(printer);

    return rc;
}

int main(int argc, char **argv)
{
    int rc;
    const char *printer_name = NULL;

    if (argc != 2)
    {
        printf("Usage: %s <printer name>\n", argv[0]);
        return -EINVAL;
    }

    printer_name = argv[1];

    printf("Printing to: %s\n", printer_name);
    rc = demo_print(printer_name, A4_PAGE_NAME);
    if (rc < 0)
    {
        printf("Failed to print\n");
        return rc;
    }

    fflush(stdout);
    fflush(stderr);

    return 0;
}
