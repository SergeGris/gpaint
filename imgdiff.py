
# https://stackoverflow.com/questions/70893502/why-does-ffmpeg-output-slightly-different-rgb-values-when-converting-to-gbrp-and

import matplotlib.pyplot as plt

diff = buffer_rgb24.astype(float) - buffer_gbrp.astype(float)
fig, (ax1, ax2, ax3) = plt.subplots(ncols=3, constrained_layout=True, figsize=(12, 2.5))
ax1.imshow(buffer_rgb24)
ax1.set_title("rgb24")
ax2.imshow(buffer_gbrp)
ax2.set_title("gbrp")
im = ax3.imshow(diff[..., 1], vmin=-5, vmax=+5, cmap="seismic")
ax3.set_title("difference (green channel)")
plt.colorbar(im, ax=ax3)
plt.show()
