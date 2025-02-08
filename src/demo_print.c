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

void draw(HDC canvas)
{
    POINT pts[4] = {
        {100, 100}, {600, 300}, {800, 100}, {1300, 300}};

    printf("First rectangle (logical): (%d, %d) - (%d, %d)\n", pts[0].x, pts[0].y, pts[1].x, pts[1].y);
    printf("Second rectangle (logical): (%d, %d) - (%d, %d)\n", pts[2].x, pts[2].y, pts[3].x, pts[3].y);

    int mapmode = GetMapMode(canvas);
    printf("Map mode before drawing: %d\n", mapmode);

    SIZE windows_ext, viewport_ext;
    if (GetWindowExtEx(canvas, &windows_ext) == 0)
    {
        fprintf(stderr, "Failed to get window extents\n");
        return;
    }

    if (GetViewportExtEx(canvas, &viewport_ext) == 0)
    {
        fprintf(stderr, "Failed to get viewport extents\n");
        return;
    }

    printf("Window extents before drawing: %d x %d\n", windows_ext.cx, windows_ext.cy);
    printf("Viewport extents before drawing: %d x %d\n", viewport_ext.cx, viewport_ext.cy);

    Rectangle(canvas, pts[0].x, pts[0].y, pts[1].x, pts[1].y);
    Rectangle(canvas, pts[2].x, pts[2].y, pts[3].x, pts[3].y);

    LPtoDP(canvas, pts, 4);
    printf("First rectangle (device): (%d, %d) - (%d, %d)\n", pts[0].x, pts[0].y, pts[1].x, pts[1].y);
    printf("Second rectangle (device): (%d, %d) - (%d, %d)\n", pts[2].x, pts[2].y, pts[3].x, pts[3].y);
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
        fprintf(stderr, "Failed to open printer\n");
        rc = -EINVAL;
        goto exit;
    }

    devmode_size = DocumentProperties(
        NULL, printer, (char *)printer_name, NULL, NULL, 0);

    if (devmode_size <= 0)
    {
        fprintf(stderr, "Failed to get printer properties size\n");
        rc = -EINVAL;
        goto exit;
    }

    *devmode = (DEVMODE *)malloc(devmode_size);
    if (*devmode == NULL)
    {
        fprintf(stderr, "Failed to allocate %u octets\n", devmode_size);
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
        fprintf(stderr, "Failed to get printer properties\n");
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
        fprintf(stderr, "Failed to set printer properties\n");
        rc = -EINVAL;
        goto exit;
    }

    if (ClosePrinter(printer) == 0)
    {
        fprintf(stderr, "Failed to close printer\n");
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

int CALLBACK EmfEnumProc(HDC hdc, HANDLETABLE *handle_table, const ENHMETARECORD *record, int handles, LPARAM data)
{
    printf("Record type: %d, size: %d\n", record->iType, record->nSize);

    if (record->iType == EMR_RECTANGLE)
    {
        EMRRECTANGLE *rect = (EMRRECTANGLE *)record;
        printf("  Rectangle: (%d, %d) - (%d, %d)\n", rect->rclBox.left, rect->rclBox.top, rect->rclBox.right, rect->rclBox.bottom);
    }
    return 1;
}

int print_emf(
    HDC printer,
    HENHMETAFILE emf,
    const char *file_name,
    const struct coordinate_space *space)
{
    DOCINFO doc_info = {0};
    RECT rect;
    char outfile[256];

    int mapmode = GetMapMode(printer);

    SIZE windows_ext, viewport_ext;
    if (GetWindowExtEx(printer, &windows_ext) == 0)
    {
        fprintf(stderr, "Failed to get window extents\n");
        return -EINVAL;
    }

    if (GetViewportExtEx(printer, &viewport_ext) == 0)
    {
        fprintf(stderr, "Failed to get viewport extents\n");
        return -EINVAL;
    }

    printf("Printer map mode: %d\n", mapmode);
    printf("Printer window extents: %d x %d\n", windows_ext.cx, windows_ext.cy);
    printf("Printer viewport extents:  %d x %d\n", viewport_ext.cx, viewport_ext.cy);

    printf("Sending %s to printer\n", file_name);

    /* We need to crop to just the printable area when rendering
     * this to the printer. */
    rect.left = space->logical.offset_x;
    rect.top = space->logical.offset_y;
    rect.right = space->logical.width - space->logical.offset_x;
    rect.bottom = space->logical.height - space->logical.offset_y;

    printf(
        "  Cropping to (%d, %d) - (%d, %d)\n",
        rect.left,
        rect.top,
        rect.right,
        rect.bottom);

    doc_info.cbSize = sizeof(doc_info);
    doc_info.lpszDocName = file_name;

    if (StartDoc(printer, &doc_info) <= 0)
    {
        fprintf(stderr, "Failed to start document\n");
        return -EINVAL;
    }

    if (StartPage(printer) <= 0)
    {
        fprintf(stderr, "Failed to start page\n");
        EndDoc(printer);
        return -EINVAL;
    }

    if (PlayEnhMetaFile(printer, emf, &rect) == 0)
    {
        fprintf(stderr, "Failed to play metafile\n");
    }

    if (EndPage(printer) <= 0)
    {
        fprintf(stderr, "Failed to end page\n");
        EndDoc(printer);
        return -EINVAL;
    }

    if (EndDoc(printer) <= 0)
    {
        fprintf(stderr, "Failed to end document\n");
        return -EINVAL;
    }

    return 0;
}

static int print_direct(HDC printer, struct coordinate_space *space)
{
    DOCINFO doc_info = {0};

    doc_info.cbSize = sizeof(doc_info);
    doc_info.lpszDocName = "Direct Print";

    printf("Skipping EMF and going straight to the printer\n");

    if (StartDoc(printer, &doc_info) <= 0)
    {
        fprintf(stderr, "Failed to start document\n");
        return -EINVAL;
    }

    if (StartPage(printer) <= 0)
    {
        fprintf(stderr, "Failed to start page\n");
        return -EINVAL;
    }

    if (SaveDC(printer) == 0)
    {
        fprintf(stderr, "Failed to save DC\n");
        return -EINVAL;
    }

    if (SetMapMode(printer, MM_ISOTROPIC) == 0)
    {
        fprintf(stderr, "Failed to set map mode\n");
        return -EINVAL;
    }

    if (SetWindowExtEx(printer, space->logical.width, space->logical.height, NULL) == 0)
    {
        fprintf(stderr, "Failed to set window extents\n");
        return -EINVAL;
    }

    if (SetViewportExtEx(printer, space->device.width, space->device.height, NULL) == 0)
    {
        fprintf(stderr, "Failed to set viewport extents\n");
        return -EINVAL;
    }

    if (SetViewportOrgEx(printer, 0, 0, NULL) == 0)
    {
        fprintf(stderr, "Failed to set viewport origin\n");
        return -EINVAL;
    }

    POINT pts[2] = {{100, 100}, {1100, 1100}};
    printf("Drawing a rectangle at (%d, %d) - (%d, %d)\n", pts[0].x, pts[0].y, pts[1].x, pts[1].y);
    Rectangle(printer, pts[0].x, pts[0].y, pts[1].x, pts[1].y);

    LPtoDP(printer, pts, 2);
    printf("Rectangle in device coordinates: (%d, %d) - (%d, %d)\n", pts[0].x, pts[0].y, pts[1].x, pts[1].y);

    if (RestoreDC(printer, -1) == 0)
    {
        fprintf(stderr, "Failed to restore DC\n");
        return -EINVAL;
    }

    if (EndPage(printer) <= 0)
    {
        fprintf(stderr, "Failed to end page\n");
        return -EINVAL;
    }

    if (EndDoc(printer) <= 0)
    {
        fprintf(stderr, "Failed to end document\n");
        return -EINVAL;
    }
}

int demo_print(const char *printer_name, const char *page_size)
{
    int rc;
    struct page_details *details = NULL;
    DEVMODE *devmode = NULL;
    struct coordinate_space space;
    HDC printer = NULL;
    HDC canvas = NULL;
    HENHMETAFILE emf = NULL;

    /* Configure the printer - for now all we're doing is setting the
     * page size. We have to do this first, because we then ask the printer
     * to tell us, based on this page size, how many pixels it has in X
     * and Y. */
    details = get_page_details(printer_name, page_size);
    if (details == NULL)
    {
        fprintf(stderr, "Failed to get page details\n");
        rc = -EINVAL;
        goto exit;
    }

    rc = set_page_size(printer_name, details, &devmode);
    if (rc < 0)
    {
        fprintf(stderr, "Failed to set page size\n");
        goto exit;
    }

    printer = CreateDC("WINSPOOL", printer_name, NULL, devmode);
    if (printer == NULL)
    {
        fprintf(stderr, "Failed to create printer\n");
        rc = -EINVAL;
        goto exit;
    }

    /* Our drawing is done in logical units and, for convenience, we'll use
     * 1/10 mm units (as returned by DC_PAPERSIZE) and represent the entire
     * page. */
    space.logical.width = details->dimensions.x;
    space.logical.height = details->dimensions.y;

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

    printf("PHYISCALWIDTH: %d\n", space.device.width);
    printf("PHYSICALHEIGHT: %d\n", space.device.height);
    printf("PHYSICALOFFSETX: %d\n", space.device.offset_x);
    printf("PHYSICALOFFSETY: %d\n", space.device.offset_y);
    printf("LOGPIXELSX: %d\n", printer_dpi_x);
    printf("LOGPIXELSY: %d\n", printer_dpi_y);
    printf("HORZRES: %d\n", printer_horz_res);
    printf("VERTRES: %d\n", printer_vert_res);

    /* We also need to calculate the offsets for our logical page. */
    double scale_x = (double)space.logical.width / (double)space.device.width;
    double scale_y = (double)space.logical.height / (double)space.device.height;

    printf("Scale factors: %f x %f\n", scale_x, scale_y);

    space.logical.offset_x = (int)(space.device.offset_x * scale_x);
    space.logical.offset_y = (int)(space.device.offset_y * scale_y);

    printf("Coordinate space:\n");

    printf(
        "  Logical: %d x %d, offset (%d, %d)\n",
        space.logical.width,
        space.logical.height,
        space.logical.offset_x,
        space.logical.offset_y);

    printf(
        "  Device: %d x %d, offset (%d, %d)\n",
        space.device.width,
        space.device.height,
        space.device.offset_x,
        space.device.offset_y);

    // /* Prepare the EMF for drawing on. */
    // canvas = CreateEnhMetaFile(NULL, EMF_FILE_NAME, NULL, NULL);
    // if (canvas == NULL)
    // {
    //     fprintf(stderr, "Failed to create metafile\n");
    //     rc = -EINVAL;
    //     goto exit;
    // }

    // if (SetGraphicsMode(canvas, GM_ADVANCED) == 0)
    // {
    //     fprintf(stderr, "Failed to set graphics mode\n");
    //     rc = -EINVAL;
    //     goto exit;
    // }

    // if (SetMapMode(canvas, MM_ISOTROPIC) == 0)
    // {
    //     fprintf(stderr, "Failed to set map mode\n");
    //     rc = -EINVAL;
    //     goto exit;
    // }

    // /* Logical space is the entire page - we use the offset while drawing. */
    // if (SetWindowExtEx(
    //         canvas, space.logical.width, space.logical.height, NULL) == 0)
    // {
    //     fprintf(stderr, "Failed to set window extents\n");
    //     rc = -EINVAL;
    //     goto exit;
    // }

    // /* Physical space is the actual printable area - and we've already
    //  * accounted for the offsets. */
    // if (SetViewportExtEx(
    //         canvas, space.device.width, space.device.height, NULL) == 0)
    // {
    //     fprintf(stderr, "Failed to set viewport extents\n");
    //     rc = -EINVAL;
    //     goto exit;
    // }

    // /* Actually draw out what we want to draw! */
    // draw(canvas);

    // emf = CloseEnhMetaFile(canvas);
    // if (emf == NULL)
    // {
    //     fprintf(stderr, "Failed to close metafile\n");
    //     rc = -EINVAL;
    //     goto exit;
    // }

    // /* Send the EMF to the printer. */
    // rc = print_emf(printer, emf, "Demo Print", &space);
    // if (rc < 0)
    // {
    //     fprintf(stderr, "Failed to print metafile\n");
    //     goto exit;
    // }

    rc = print_direct(printer, &space);
    if (rc < 0)
    {
        fprintf(stderr, "Failed to print directly\n");
        goto exit;
    }

    rc = 0;

exit:
    if (details != NULL)
        free(details);

    if (printer != NULL)
        DeleteDC(printer);

    if (emf != NULL)
        DeleteEnhMetaFile(emf);

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
