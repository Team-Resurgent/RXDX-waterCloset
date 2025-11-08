#include "draw.h"

static void initColor(SDL_Color* c, int r, int g, int b);

/* === DISABLE render targets on OG Xbox to avoid XPhysicalAlloc assert === */
static void log_renderer_info(SDL_Renderer* r)
{
    SDL_RendererInfo inf;
    if (SDL_GetRendererInfo(r, &inf) == 0) {
        SDL_Log("Renderer: %s  flags=0x%08X  (TARGETTEXTURE=%d)",
            inf.name ? inf.name : "(null)",
            (unsigned)inf.flags,
            (inf.flags & SDL_RENDERER_TARGETTEXTURE) != 0);

        SDL_Log("Texture formats reported (%u):", (unsigned)inf.num_texture_formats);
        for (Uint32 i = 0; i < inf.num_texture_formats; ++i) {
            SDL_Log("  [%u] %s (0x%08X)",
                (unsigned)i,
                SDL_GetPixelFormatName(inf.texture_formats[i]),
                (unsigned)inf.texture_formats[i]);
        }
    }
    else {
        SDL_Log("SDL_GetRendererInfo failed: %s", SDL_GetError());
    }

    int lw = 0, lh = 0; SDL_RenderGetLogicalSize(r, &lw, &lh);
    float sx = 0.f, sy = 0.f; SDL_RenderGetScale(r, &sx, &sy);
    int ow = 0, oh = 0; SDL_GetRendererOutputSize(r, &ow, &oh);
    SDL_Log("Output=%dx%d  Logical=%dx%d  Scale=%g,%g",
        ow, oh, lw, lh, (double)sx, (double)sy);
}

void initGraphics(void)
{
    initColor(&app.colors.red, 255, 0, 0);
    initColor(&app.colors.orange, 255, 128, 0);
    initColor(&app.colors.yellow, 255, 255, 0);
    initColor(&app.colors.green, 0, 255, 0);
    initColor(&app.colors.blue, 0, 0, 255);
    initColor(&app.colors.cyan, 0, 255, 255);
    initColor(&app.colors.purple, 255, 0, 255);
    initColor(&app.colors.white, 255, 255, 255);
    initColor(&app.colors.black, 0, 0, 0);
    initColor(&app.colors.lightGrey, 192, 192, 192);
    initColor(&app.colors.darkGrey, 128, 128, 128);

    /* Just log capabilities; DO NOT attempt to bind a target texture on Xbox */
    log_renderer_info(app.renderer);

    /* Ensure the code elsewhere never tries to use app.backBuffer as a target */
    app.backBuffer = NULL;
}

void prepareScene(void)
{
    /* Always render to the real backbuffer to avoid the assert */
    SDL_SetRenderTarget(app.renderer, NULL);

    SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);
    SDL_RenderClear(app.renderer);
}

void presentScene(void)
{
    if (app.dev.debug) {
        drawText(SCREEN_WIDTH - 5, SCREEN_HEIGHT - 30, 32, TEXT_RIGHT,
            app.colors.white, "%dfps | Ents: %d | Cols: %d | Draw: %d",
            app.dev.fps, app.dev.ents, app.dev.collisions, app.dev.drawing);
    }

    /* Nothing to blit—we drew directly to the backbuffer */
    SDL_RenderPresent(app.renderer);
}

void blit(SDL_Texture* texture, int x, int y, int center, SDL_RendererFlip flip)
{
    SDL_Rect dest;
    dest.x = x;
    dest.y = y;
    SDL_QueryTexture(texture, NULL, NULL, &dest.w, &dest.h);
    if (center) {
        dest.x -= dest.w / 2;
        dest.y -= dest.h / 2;
    }
    SDL_RenderCopyEx(app.renderer, texture, NULL, &dest, 0, NULL, flip);
}

void blitAtlasImage(AtlasImage* atlasImage, int x, int y, int center, SDL_RendererFlip flip)
{
    SDL_Rect dest;
    dest.x = x; dest.y = y;
    dest.w = atlasImage->rect.w; dest.h = atlasImage->rect.h;
    if (center) {
        dest.x -= dest.w / 2;
        dest.y -= dest.h / 2;
    }
    SDL_RenderCopyEx(app.renderer, atlasImage->texture, &atlasImage->rect, &dest, 0, NULL, flip);
}

void drawRect(int x, int y, int w, int h, int r, int g, int b, int a)
{
    SDL_Rect rect = (SDL_Rect){ x, y, w, h };
    SDL_SetRenderDrawBlendMode(app.renderer, a < 255 ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(app.renderer, r, g, b, a);
    SDL_RenderFillRect(app.renderer, &rect);
}

void drawOutlineRect(int x, int y, int w, int h, int r, int g, int b, int a)
{
    SDL_Rect rect = (SDL_Rect){ x, y, w, h };
    SDL_SetRenderDrawBlendMode(app.renderer, a < 255 ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(app.renderer, r, g, b, a);
    SDL_RenderDrawRect(app.renderer, &rect);
}

static void initColor(SDL_Color* c, int r, int g, int b)
{
    memset(c, 0, sizeof(SDL_Color));
    c->r = r; c->g = g; c->b = b; c->a = 255;
}
