# Use Debian 12 "Bookworm" as the base image
FROM debian:bookworm

# Install any dependencies you might need.
# In this example, we install bash and any other packages you require.
RUN apt-get update && apt-get install -y libgtk-4-dev libadwaita-1-dev desktop-file-utils pkg-config gettext meson \
    bash \
    && rm -rf /var/lib/apt/lists/*

# Copy the directory "mydir" from the host into /app in the container
COPY paint /paint

# Set the working directory
WORKDIR /paint

# Start an interactive bash shell
CMD ["/bin/bash"]
