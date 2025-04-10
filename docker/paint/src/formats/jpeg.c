#include <cairo.h>
#include <dlfcn.h>
#include <jpeglib.h>
#include <png.h>

// Function to convert PNG to Cairo surface
CairoSurface* png_to_cairo_surface(const char* filename) {
    // Dynamically load the libpng library
    void* libpng = dlopen("libpng.so", RTLD_LAZY);
    if (!libpng) {
        return NULL; // error loading libpng
    }

    // Initialize the Cairo surface
    CairoSurface* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);

    // Open the PNG file
    FILE* f = fopen(filename, "rb");
    if (!f) {
        return NULL; // error opening file
    }

    // Initialize libpng
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_MAJOR, PNG_LIBPNG_VER_MINOR);
    if (!png_ptr) {
        return NULL; // error creating libpng structure
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, (png_infopp)NULL, NULL);
        fclose(f);
        dlclose(libpng); // close the libpng library
        return NULL; // error creating libpng structure
    }

    // Read the PNG file header
    png_byte header[8];
    fread(header, 1, 8, f);

    // Check if the file is a PNG file
    if (png_sigokay(png_ptr, info_ptr, header)) {
        // Create a Cairo image from the PNG data
        unsigned int width = info_ptr->width;
        unsigned int height = info_ptr->height;
        cairo_surface_t* surface_ptr = NULL;

        for (int y = 0; y < height; y++) {
            png_byte row[width * 4];
            fread(row, 1, width * 4, f);
            if (!row) {
                png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
                fclose(f);
                dlclose(libpng); // close the libpng library
                return NULL; // error reading PNG data
            }

            surface_ptr = cairo_image_surface_create_for_data(surface, CAIRO_FORMAT_ARGB32, width,
1, width * 4, row);
        }

        if (!surface_ptr) {
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            fclose(f);
            dlclose(libpng); // close the libpng library
            return NULL; // error creating Cairo surface
        }

        // Set the Cairo surface dimensions
        cairo_surface_set_size(surface, width, height);

        // Clean up
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(f);

        dlclose(libpng); // close the libpng library

        return surface;
    } else {
        // Not a PNG file
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(f);
        dlclose(libpng); // close the libpng library
        return NULL; // error reading PNG data
    }
}

// Function to convert JPEG to Cairo surface
CairoSurface* jpeg_to_cairo_surface(const char* filename) {
    // Dynamically load the libjpeg library
    void* libjpeg = dlopen("libjpeg.so", RTLD_LAZY);
    if (!libjpeg) {
      g_error ("Failed to load libjpeg");
        return NULL; // error loading libjpeg
    }

    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);

    FILE* f = fopen(filename, "rb");
    if (!f) {
        return NULL;
    }

    // Read the JPEG file header
    struct jpeg_decompress_struct cinfo;
    JSAMPROW row_pointer[1];
    unsigned long int temp_row[1];

    // Decompress the JPEG image data
    ((void (*)(struct jpeg_decompress_struct*))dlsym(libjpeg, "jpeg_create_decompress"))(&cinfo);
    cinfo.input_buffer = f;
    cinfo.next_input_byte = f.tellg();
    cinfo.image_width = 0;
    cinfo.image_height = 0;

    // Read the JPEG image data
    while (1) {
        cinfo.output_image = FALSE;
        jcco_setup_decompress(&cinfo, row_pointer);
        temp_row[0] = 0;
        cinfo.row_pointers = &temp_row[0];
        ((void (*)(struct jpeg_decompress_struct*, JSAMPROW*))dlsym(libjpeg,
"jpeg_read_scanlines"))(&cinfo, row_pointer, 1);

        // Create a Cairo image from the JPEG data
        cairo_image_surface_create_for_data(surface, CAIRO_FORMAT_ARGB32, cinfo.image_width,
cinfo.image_height, cinfo.image_width * 4, row_pointer[0]);

        break;
    }

    // Clean up
    ((void (*)(struct jpeg_decompress_struct*))dlsym(libjpeg, "jpeg_destroy_decompress"))(&cinfo);
    fclose(f);

    dlclose(libjpeg); // close the libjpeg library

    return surface;
}
