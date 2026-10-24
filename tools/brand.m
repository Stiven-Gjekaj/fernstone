// brand: draw the logo, the app icons, and the banner of Fernstone.
//
// The art is a mossy cobblestone block in pixel art, drawn for Fernstone.
// It is not a texture of Mojang. The 16 x 16 face comes from the map below.
// The tool draws the block as an isometric cube with a soft glow behind it.
//
// Usage: brand OUTPUT_DIR
// It writes logo.png, icon-1024.png, icon-ios6.png (with the rounded corners
// and the gloss of iOS 6), icon-mac.png, and banner.png.

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

// The face of the block. '.' is the gap between the stones, a b c are the
// stone from light to dark, and G M D are the moss from light to dark.
static const char *face_map[16] = {
    "aab.GGMb.abbc.ab",
    "abbc.GMMc.bbcc.b",
    "bbcc..DMc..ccc..",
    "....ab....Gab...",
    ".aGb.abbc.GMMbc.",
    "aGMMcbbcc.DMDcc.",
    "bMDcc.ccc..ccc..",
    "......c..b......",
    "ab.aabbc.abbGGa.",
    "bbcabbbcc.bcGMMb",
    "cccbbccc..ccMDDc",
    ".cc..cc.GG..ccc.",
    "a....b.GMMa.....",
    "bbc.abbMDDbbca.a",
    "bcc.bbcccc.bcc.b",
    "....ccc..c...c..",
};

static void color_of(char c, float shade, float out[4]) {
    float r, g, b;
    switch (c) {
    case 'a': r = 0.60f; g = 0.60f; b = 0.62f; break;
    case 'b': r = 0.49f; g = 0.49f; b = 0.51f; break;
    case 'c': r = 0.38f; g = 0.38f; b = 0.40f; break;
    case 'G': r = 0.45f; g = 0.66f; b = 0.24f; break;
    case 'M': r = 0.32f; g = 0.52f; b = 0.18f; break;
    case 'D': r = 0.22f; g = 0.38f; b = 0.12f; break;
    default: r = 0.20f; g = 0.20f; b = 0.22f; break;
    }
    out[0] = r * shade;
    out[1] = g * shade;
    out[2] = b * shade;
    out[3] = 1;
}

// Draw the 16 x 16 face into the unit square of the current transform.
static void draw_face(CGContextRef cg, float shade) {
    int x, y;
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++) {
            float c[4];
            color_of(face_map[y][x], shade, c);
            CGContextSetRGBFillColor(cg, c[0], c[1], c[2], c[3]);
            // A small overlap hides the seams between the cells.
            CGContextFillRect(cg, CGRectMake(x / 16.0, 1 - (y + 1) / 16.0, 1 / 16.0 + 0.002, 1 / 16.0 + 0.002));
        }
}

// Draw the isometric cube with its center at (cx, cy) and an edge of size e.
static void draw_cube(CGContextRef cg, CGFloat cx, CGFloat cy, CGFloat e) {
    CGFloat w = e * 0.866, h = e * 0.5;
    CGAffineTransform t;
    // The top face: from the left corner to the back and to the front.
    CGContextSaveGState(cg);
    t = CGAffineTransformMake(w, h, w, -h, cx - w, cy + h);
    CGContextConcatCTM(cg, t);
    draw_face(cg, 1.0f);
    CGContextRestoreGState(cg);
    // The left face.
    CGContextSaveGState(cg);
    t = CGAffineTransformMake(w, -h, 0, e, cx - w, cy - e + h);
    CGContextConcatCTM(cg, t);
    draw_face(cg, 0.78f);
    CGContextRestoreGState(cg);
    // The right face.
    CGContextSaveGState(cg);
    t = CGAffineTransformMake(w, h, 0, e, cx, cy - e);
    CGContextConcatCTM(cg, t);
    draw_face(cg, 0.6f);
    CGContextRestoreGState(cg);
}

static CGContextRef canvas(int w, int h) {
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef cg = CGBitmapContextCreate(NULL, (size_t)w, (size_t)h, 8, 0, space, (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(space);
    CGContextSetInterpolationQuality(cg, kCGInterpolationNone);
    CGContextSetShouldAntialias(cg, true);
    return cg;
}

static void save(CGContextRef cg, NSString *path) {
    CGImageRef image = CGBitmapContextCreateImage(cg);
    CGImageDestinationRef d = CGImageDestinationCreateWithURL((__bridge CFURLRef)[NSURL fileURLWithPath:path],
                                                              (__bridge CFStringRef)UTTypePNG.identifier, 1, NULL);
    CGImageDestinationAddImage(d, image, NULL);
    CGImageDestinationFinalize(d);
    CFRelease(d);
    CGImageRelease(image);
    printf("%s\n", path.fileSystemRepresentation);
}

static void glow(CGContextRef cg, CGFloat cx, CGFloat cy, CGFloat r, float alpha) {
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGFloat comps[8] = {0.45, 0.85, 0.35, alpha, 0.45, 0.85, 0.35, 0};
    CGGradientRef g = CGGradientCreateWithColorComponents(space, comps, NULL, 2);
    CGContextDrawRadialGradient(cg, g, CGPointMake(cx, cy), 0, CGPointMake(cx, cy), r, 0);
    CGGradientRelease(g);
    CGColorSpaceRelease(space);
}

static void background(CGContextRef cg, int w, int h) {
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGFloat comps[8] = {0.10, 0.14, 0.12, 1, 0.03, 0.05, 0.05, 1};
    CGGradientRef g = CGGradientCreateWithColorComponents(space, comps, NULL, 2);
    CGContextDrawLinearGradient(cg, g, CGPointMake(0, h), CGPointMake(0, 0), 0);
    CGGradientRelease(g);
    CGColorSpaceRelease(space);
}

// The icon: the cube on a dark square, with a green glow.
static CGContextRef icon(int size) {
    CGContextRef cg = canvas(size, size);
    background(cg, size, size);
    glow(cg, size * 0.5, size * 0.5, size * 0.55, 0.45f);
    draw_cube(cg, size * 0.5, size * 0.5, size * 0.36);
    return cg;
}

// The rounded corners and the top gloss of an iOS 6 icon, so that iOS adds
// no gloss of its own (UIPrerenderedIcon).
static CGContextRef ios6_icon(int size) {
    CGContextRef src = icon(size), cg = canvas(size, size);
    CGImageRef image = CGBitmapContextCreateImage(src);
    CGFloat r = size * 10.0 / 57.0;
    CGPathRef path = CGPathCreateWithRoundedRect(CGRectMake(0, 0, size, size), r, r, NULL);
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGFloat comps[8] = {1, 1, 1, 0.38, 1, 1, 1, 0.08};
    CGGradientRef g = CGGradientCreateWithColorComponents(space, comps, NULL, 2);
    CGContextAddPath(cg, path);
    CGContextClip(cg);
    CGContextDrawImage(cg, CGRectMake(0, 0, size, size), image);
    CGContextSaveGState(cg);
    CGContextAddEllipseInRect(cg, CGRectMake(-size * 0.5, size * 0.52, size * 2.0, size * 0.9));
    CGContextClip(cg);
    CGContextDrawLinearGradient(cg, g, CGPointMake(0, size), CGPointMake(0, size * 0.5), 0);
    CGContextRestoreGState(cg);
    CGGradientRelease(g);
    CGColorSpaceRelease(space);
    CGPathRelease(path);
    CGImageRelease(image);
    CGContextRelease(src);
    return cg;
}

// The icon of macOS: a rounded square with a margin, as the system draws them.
static CGContextRef mac_icon(int size) {
    CGContextRef src = icon(size), cg = canvas(size, size);
    CGImageRef image = CGBitmapContextCreateImage(src);
    CGFloat m = size * 0.1, s = size - 2 * m, r = s * 0.225;
    CGPathRef path = CGPathCreateWithRoundedRect(CGRectMake(m, m, s, s), r, r, NULL);
    CGContextSetShadowWithColor(cg, CGSizeMake(0, -size * 0.01), size * 0.02, CGColorGetConstantColor(kCGColorBlack));
    CGContextAddPath(cg, path);
    CGContextSetRGBFillColor(cg, 0, 0, 0, 1);
    CGContextFillPath(cg);
    CGContextSetShadowWithColor(cg, CGSizeZero, 0, NULL);
    CGContextAddPath(cg, path);
    CGContextClip(cg);
    CGContextDrawImage(cg, CGRectMake(m, m, s, s), image);
    CGPathRelease(path);
    CGImageRelease(image);
    CGContextRelease(src);
    return cg;
}

// The pixel letters of the wordmark: the 5 x 7 glyphs of the built-in font.
static const char *glyph(char c) {
    switch (c) {
    case 'F': return "11111100001000011110100001000010000";
    case 'E': return "11111100001000011110100001000011111";
    case 'R': return "11110100011000111110101001001010001";
    case 'N': return "10001100011100110101100111000110001";
    case 'S': return "01111100001000001110000010000111110";
    case 'T': return "11111001000010000100001000010000100";
    case 'O': return "01110100011000110001100011000101110";
    default: return "00000000000000000000000000000000000";
    }
}

static void wordmark(CGContextRef cg, const char *text, CGFloat x, CGFloat y, CGFloat px) {
    for (; *text; text++, x += 6 * px) {
        const char *g = glyph(*text);
        int r, c;
        for (r = 0; r < 7; r++)
            for (c = 0; c < 5; c++)
                if (g[r * 5 + c] == '1') {
                    CGFloat bx = x + c * px, by = y + (6 - r) * px;
                    // The shadow, then the pixel with a light top.
                    CGContextSetRGBFillColor(cg, 0.05, 0.08, 0.05, 1);
                    CGContextFillRect(cg, CGRectMake(bx + px * 0.25, by - px * 0.25, px, px));
                    CGContextSetRGBFillColor(cg, 0.86, 0.92, 0.82, 1);
                    CGContextFillRect(cg, CGRectMake(bx, by, px, px));
                }
    }
}

int main(int argc, char **argv) {
    @autoreleasepool {
        NSString *dir;
        CGContextRef cg;
        if (argc < 2) {
            fprintf(stderr, "usage: brand OUTPUT_DIR\n");
            return 2;
        }
        dir = @(argv[1]);
        // The logo: the cube with a glow, on a clear background.
        cg = canvas(512, 512);
        glow(cg, 256, 256, 250, 0.5f);
        draw_cube(cg, 256, 256, 190);
        save(cg, [dir stringByAppendingPathComponent:@"logo.png"]);
        CGContextRelease(cg);
        cg = icon(1024);
        save(cg, [dir stringByAppendingPathComponent:@"icon-1024.png"]);
        CGContextRelease(cg);
        cg = ios6_icon(114);
        save(cg, [dir stringByAppendingPathComponent:@"icon-ios6.png"]);
        CGContextRelease(cg);
        cg = mac_icon(1024);
        save(cg, [dir stringByAppendingPathComponent:@"icon-mac.png"]);
        CGContextRelease(cg);
        // The banner: the cube and the wordmark.
        cg = canvas(1280, 400);
        background(cg, 1280, 400);
        glow(cg, 250, 200, 260, 0.45f);
        draw_cube(cg, 250, 200, 140);
        wordmark(cg, "FERNSTONE", 440, 155, 13);
        save(cg, [dir stringByAppendingPathComponent:@"banner.png"]);
        CGContextRelease(cg);
    }
    return 0;
}
