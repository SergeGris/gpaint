CairoSurface* png_to_cairo_surface(const char* filename) {
    // Initialize the Cairo surface
    CairoSurface* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);

    // Open the PNG file
    FILE* f = fopen(filename, "rb");
    if (!f) {
        return NULL; // error opening file
    }

    // Read the PNG file header
    png_structp png_ptr;
    png_infop info_ptr;
    png_ptr = png_create_read_struct(PNG_SIGNATURE_TYPE, NULL, NULL);
    info_ptr = png_create_info_struct(png_ptr);

    // Set up the PNG reader
    png_set_read_explicit(png_ptr, info_ptr, f);
    png_set_strip_alpha(png_ptr);
    png_set_filer(pimg_ptr);

    // Read the PNG image data
    int width, height;
    png_get_IHDR(png_ptr, info_ptr, &width, &height, NULL, NULL, NULL, NULL, NULL);
    unsigned char* buffer = (unsigned char*)malloc(width * height * 4);
    png_read_image(png_ptr, buffer);

    // Create a Cairo image from the PNG data
    cairo_image_surface_create_for_data(surface, CAIRO_FORMAT_ARGB32, width, height, 4 * width,
buffer);

    // Clean up
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    free(buffer);
    fclose(f);

    return surface;
}
