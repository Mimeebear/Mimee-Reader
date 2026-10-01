# Boot logo assets

`boot-logo.jpg` is the 890x890 source. The firmware embeds only the generated
1-bpp `src/images/BootLogo.h`; the original JPEG and previews are not included
in the firmware.

The shared logo is 400x400, centered using the active runtime screen size. The
400px candidate preserves the line art at threshold 128 and fits above the
boot/sleep labels on both X4 and X3 portrait screens. A 360px candidate was
visually compared; 400px remained clear without touching the text. No dithering
is used because the artwork is line art without gradients.

Regenerate the header and both boot previews from the repository root:

```bash
version=$(python3 scripts/git_branch.py | sed -n 's/^CrossPoint build version: //p')
common=(assets/boot-logo/boot-logo.jpg boot_logo 400 400 --fit-contain --no-rotate --threshold 128 --header-output src/images/BootLogo.h --array-name BootLogo --emit-dimensions --preview-title CrossPoint --preview-status BOOTING --preview-version "$version" --preview-title-font lib/EpdFont/builtinFonts/source/Ubuntu/Ubuntu-Bold.ttf --preview-small-font lib/EpdFont/builtinFonts/source/NotoSans/NotoSans-Regular.ttf)
python3 scripts/convert_icon.py "${common[@]}" --preview assets/boot-logo/preview-x4.png --screen-size 480x800
python3 scripts/convert_icon.py "${common[@]}" --preview assets/boot-logo/preview-x3.png --screen-size 528x792
```