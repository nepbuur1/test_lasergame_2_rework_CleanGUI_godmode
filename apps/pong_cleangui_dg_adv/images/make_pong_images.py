#!/usr/bin/env python3
"""Tekent de bron-PNG's van pong_cleangui_dg_adv.

De PNG's staan in git en zijn het echte bronmateriaal - dit script is er om ze
te kunnen bijstellen zonder een tekenprogramma, en om vast te leggen waar de
maten vandaan komen. Vervang gerust een PNG door handwerk; zolang de afmeting
klopt merkt de app er niets van.

    python3 apps/pong_cleangui_dg_adv/images/make_pong_images.py
    python3 tools/img2rgb565.py --pal8 apps/pong_cleangui_dg_adv/images

De maten zijn geen smaak maar volgen uit pdga_GameDisplayControl.h: scherm
320x240 (rotatie 1), speelveld 320x180, WallThicknessPix 10, BallSizePix 12,
PaddleWidthPix 50, PaddleHeightPix 12. Loopt daar een constante uit de pas, dan
past het plaatje niet meer op zijn panel: een opaak plaatje dat kleiner is dan
zijn panel laat Panel::needsBackground() de achtergrond er alsnog onder
schilderen, en dat zie je meteen.

Randen en doellijn zijn opaak - ze liggen op de zwarte achtergrond en dekken hun
hele rechthoek. Bal, peddel, knop en "GAME OVER" hebben ronde vormen en dragen
daarom -trcolor_FF00FF in hun naam: elke pixel met alpha < 128 wordt palet-index
255 en laat door wat eronder ligt.
"""

import math
import os

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))

# Het antialiasen gebeurt door 4x zo groot te tekenen en te verkleinen.
SS = 4

FONT_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

# --- kleuren ---------------------------------------------------------------
# Dezelfde kleuren staan in _pal8/_userPalette.h. Daar liggen ze op een vaste
# index, zodat de kleuren die de code zelf nog noemt (de zwarte achtergrond, de
# rode peddelknoppen) exact in het palet zitten in plaats van "iets dat er dicht
# bij ligt".
BLACK        = (0x00, 0x00, 0x00)
CYAN         = (0x00, 0xFF, 0xFF)
CYAN_MID     = (0x00, 0xB4, 0xC8)
CYAN_DARK    = (0x00, 0x50, 0x70)
YELLOW       = (0xFF, 0xF0, 0x40)
YELLOW_LIT   = (0xFF, 0xFF, 0xC0)
YELLOW_DARK  = (0xA0, 0x70, 0x00)
GREY_LINE    = (0x40, 0x40, 0x40)
RED          = (0xE0, 0x20, 0x20)
RED_LIT      = (0xFF, 0x80, 0x70)
RED_DARK     = (0x70, 0x00, 0x00)
BOX_DARK     = (0x18, 0x18, 0x20)
GAMEOVER_RED = (0xFF, 0x40, 0x40)
WHITE        = (0xFF, 0xFF, 0xFF)


def lerp(colour_a, colour_b, t):
    return tuple(int(round(a + (b - a) * t)) for a, b in zip(colour_a, colour_b))


def save(img, name):
    path = os.path.join(HERE, name)
    img.save(path)
    print("  %s (%d x %d)" % (name, img.width, img.height))


def antialiased(width, height, base_colour, draw_colour, draw_mask):
    """Tekent op 4x en verkleint - kleur en dekking apart.

    Waarom apart: verkleinen van een RGBA-plaatje mengt de kleur van de rand met
    de doorzichtige pixels ernaast, en die zijn zwart. Dan krijgt elke vorm een
    donkere zoom. Door de kleurlaag over het hele vlak door te tekenen en de vorm
    alleen in het masker te zetten, ziet het verkleinen nooit een lege pixel.
    """

    colour = Image.new("RGB", (width * SS, height * SS), base_colour)
    mask = Image.new("L", (width * SS, height * SS), 0)

    # De tekenaars krijgen het Image en niet alleen een ImageDraw: een enkele
    # vorm die zelf uit meerdere lagen bestaat - de bal met zijn eigen verloop
    # en masker - is dan als tegel in te plakken zonder de rest te overschilderen.
    draw_colour(colour, width * SS, height * SS)
    draw_mask(mask, width * SS, height * SS)

    colour = colour.resize((width, height), Image.LANCZOS)
    mask = mask.resize((width, height), Image.LANCZOS)

    out = colour.convert("RGBA")
    out.putalpha(mask)
    return out


def font_that_fits(text, width, height):
    """De grootste DejaVu Bold die met wat marge binnen (width, height) past."""

    for size in range(height, 5, -1):
        font = ImageFont.truetype(FONT_PATH, size)
        left, top, right, bottom = font.getbbox(text)
        if (right - left) <= width and (bottom - top) <= height:
            return font
    return ImageFont.truetype(FONT_PATH, 6)


def draw_centred_text(draw, box, text, font, fill, stroke_width=0, stroke_fill=None):
    x0, y0, x1, y1 = box
    left, top, right, bottom = draw.textbbox((0, 0), text, font=font,
                                             stroke_width=stroke_width)
    x = x0 + ((x1 - x0) - (right - left)) // 2 - left
    y = y0 + ((y1 - y0) - (bottom - top)) // 2 - top
    draw.text((x, y), text, font=font, fill=fill,
              stroke_width=stroke_width, stroke_fill=stroke_fill)


# --- de wanden -------------------------------------------------------------
# Een verloop dwars op de wand: donker aan de buitenkant, licht in het midden.
# Dat leest als een ronding, en het is precies het soort detail dat een
# fillRect niet kan en een plaatje gratis meebrengt.

def wall_strip(length, thickness, horizontal):
    img = Image.new("RGB", (length, thickness) if horizontal else (thickness, length))
    draw = ImageDraw.Draw(img)

    for i in range(thickness):
        # 0 aan de buitenrand, 1 in het midden van de wand.
        t = 1.0 - abs((i + 0.5) / thickness - 0.5) * 2.0
        colour = lerp(CYAN_DARK, CYAN, t ** 0.7)
        if horizontal:
            draw.line([(0, i), (length - 1, i)], fill=colour)
        else:
            draw.line([(i, 0), (i, length - 1)], fill=colour)

    # Voegen om de 24 pixels, zodat de wand niet één gladde balk is.
    for pos in range(0, length, 24):
        if horizontal:
            draw.line([(pos, 0), (pos, thickness - 1)], fill=CYAN_DARK)
        else:
            draw.line([(0, pos), (thickness - 1, pos)], fill=CYAN_DARK)

    return img


def make_walls(playfield_width, playfield_height, thickness):
    left = wall_strip(playfield_height, thickness, horizontal=False)
    save(left, "borderLeft.png")

    # De rechterwand is de linker gespiegeld, zodat het verloop bij beide naar
    # het speelveld toe wijst in plaats van bij een van de twee de verkeerde
    # kant op te lopen.
    save(left.transpose(Image.FLIP_LEFT_RIGHT), "borderRight.png")

    save(wall_strip(playfield_width, thickness, horizontal=True), "borderTop.png")


# --- de doellijn -----------------------------------------------------------

def make_goal_line(width, height):
    img = Image.new("RGB", (width, height), BLACK)
    draw = ImageDraw.Draw(img)

    # Streepjes in plaats van een doorgetrokken lijn: dit is de rand waar de bal
    # uit valt, geen wand, en zo ziet het er ook naar uit.
    for x in range(0, width, 12):
        draw.rectangle([x, 0, x + 6, height - 1], fill=GREY_LINE)

    save(img, "goalLine.png")


# --- de bal ----------------------------------------------------------------
#
# Een spritesheet van zes cellen op een rij: cel 0 is de gewone bal, cel 1..5 is
# een cyclus van een brandende bal. Zie UC05_BallHandling in
# models/PongDesign_adv.md - een bal die de onderlijn passeert vliegt brandend
# door tot hij het scherm uit is.
#
# De cel is ruimer dan de bal (BallSizePix), zodat de vlammen ergens heen kunnen
# zonder dat de botsingsmaat verandert. De bal staat in het MIDDEN van de cel:
# GameDisplayControl plaatst het panel op het balmiddelpunt, dus een bal die niet
# in het midden van zijn cel staat, staat op het scherm ook naast waar het spel
# hem denkt te hebben.

# De voet bewust geen bijna-wit: op deze maat steekt hij naast de bal uit en
# leest wit daar als een omlijning om de bal in plaats van als vuur.
FLAME_HOT  = (0xFF, 0xE0, 0x60)
FLAME_MID  = (0xFF, 0xA0, 0x20)
FLAME_COOL = (0xF0, 0x60, 0x10)
FLAME_TIP  = (0xB0, 0x20, 0x00)


def flame_colour(t):
    """Van wit-heet bij de bal naar dieprood in de punt."""

    if t < 0.35:
        return lerp(FLAME_HOT, FLAME_MID, t / 0.35)
    if t < 0.7:
        return lerp(FLAME_MID, FLAME_COOL, (t - 0.35) / 0.35)
    return lerp(FLAME_COOL, FLAME_TIP, (t - 0.7) / 0.3)


def draw_ball_body(draw, cx, cy, radius):
    # Radiaal verloop met het licht linksboven: van buiten naar binnen steeds
    # lichter, en de buitenste ring donker.
    lightX, lightY = cx - radius * 0.24, cy - radius * 0.28
    steps = 24
    for i in range(steps, 0, -1):
        t = i / steps
        r = radius * 1.64 * t
        draw.ellipse([lightX - r, lightY - r, lightX + r, lightY + r],
                     fill=lerp(YELLOW_LIT, YELLOW_DARK, t))
    draw.ellipse([cx - radius, cy - radius, cx + radius, cy + radius],
                 outline=YELLOW_DARK, width=SS)


def draw_flames(draw, cx, cy, ball_radius, phase, colour_layer):
    """Vlamtongen boven de bal.

    Boven, want de brandende bal valt naar beneden - vuur trekt dan de andere
    kant op. De vijf tongen krijgen elk een eigen faseverschuiving, zodat ze niet
    als een blok op en neer gaan maar door elkaar heen flakkeren.

    De punt wordt als eerste getekend en de hete voet als laatste, zodat het
    lichte deel bovenop komt te liggen in plaats van eronder te verdwijnen.
    """

    tongues = [(-0.62, 0.70), (-0.30, 0.90), (0.0, 1.0), (0.32, 0.88), (0.62, 0.66)]

    for k, (offset, heightScale) in enumerate(tongues):
        wobble = math.sin(2.0 * math.pi * (phase + k / len(tongues)))

        # 1,6 x de balstraal is geen smaak maar de ruimte die er is: de bal staat
        # midden in de cel, dus boven hem past (cel/2 - balstraal) = 8 pixels. Een
        # hogere vlam wordt door de celrand afgesneden, en dat zie je als een
        # rechte streep dwars door het vuur.
        height = ball_radius * 1.6 * heightScale * (0.72 + 0.28 * wobble)

        steps = 16
        for s in range(steps - 1, -1, -1):
            t = s / (steps - 1)
            x = cx + offset * ball_radius * (1.0 - 0.35 * t) \
                + math.sin(t * 3.4 + 2.0 * math.pi * phase) * ball_radius * 0.22 * t
            y = cy - ball_radius * 0.55 - height * t
            r = ball_radius * 0.40 * (1.0 - t) ** 0.75 + 0.5 * SS

            box = [x - r, y - r, x + r, y + r]
            draw.ellipse(box, fill=flame_colour(t) if colour_layer else 255)


def ball_cell(size, phase):
    """Een cel van de spritesheet. phase is None voor de gewone bal."""

    ballDiameter = 12		# BallSizePix - de bal zelf blijft even groot.

    def geometry(w, h):
        cx, cy = w / 2.0, h / 2.0
        ballRadius = (ballDiameter / 2.0) * (w / size)
        return cx, cy, ballRadius

    def colour(img, w, h):
        cx, cy, ballRadius = geometry(w, h)
        draw = ImageDraw.Draw(img)

        if phase is not None:
            draw_flames(draw, cx, cy, ballRadius, phase, True)

        # De bal als eigen tegel met eigen masker. Zijn radiale verloop wordt
        # getekend met cirkels die tot ruim buiten de bal reiken - dat is wat
        # het licht van opzij laat komen - en zonder dit masker schilderde dat
        # verloop de vlammen eromheen weer geel.
        side = max(2, int(round(2 * ballRadius)))
        body = Image.new("RGB", (side, side), YELLOW)
        draw_ball_body(ImageDraw.Draw(body), side / 2.0, side / 2.0, side / 2.0)

        bodyMask = Image.new("L", (side, side), 0)
        ImageDraw.Draw(bodyMask).ellipse([0, 0, side - 1, side - 1], fill=255)

        img.paste(body, (int(round(cx - ballRadius)), int(round(cy - ballRadius))), bodyMask)

    def mask(img, w, h):
        cx, cy, ballRadius = geometry(w, h)
        draw = ImageDraw.Draw(img)

        if phase is not None:
            draw_flames(draw, cx, cy, ballRadius, phase, False)

        draw.ellipse([cx - ballRadius, cy - ballRadius,
                      cx + ballRadius, cy + ballRadius], fill=255)

    base = YELLOW if phase is None else FLAME_MID
    return antialiased(size, size, base, colour, mask)


def make_ball_sheet(cell_size, nof_burning):
    cells = [ball_cell(cell_size, None)]
    for i in range(nof_burning):
        cells.append(ball_cell(cell_size, i / nof_burning))

    sheet = Image.new("RGBA", (cell_size * len(cells), cell_size), (0, 0, 0, 0))
    for i, cell in enumerate(cells):
        sheet.paste(cell, (i * cell_size, 0))

    # -ssh_1x6: een rij van zes cellen. De converter zet dat in Image::nofRows /
    # nofCols, en Panel::setImageFrame() kiest er een uit.
    save(sheet, "ball-trcolor_FF00FF-ssh_1x%d.png" % len(cells))


# --- de present crate ------------------------------------------------------
#
# Een spritesheet van vijf cellen: cel 0 is gaaf, cel 4 is bijna stuk. De crate
# begint met vijf treffers tegoed (UC07), dus de cel volgt uit de teller en de
# speler ziet zonder cijfers hoe ver hij is.

CRATE_WOOD      = (0xA8, 0x70, 0x38)
CRATE_WOOD_LIT  = (0xD8, 0xA0, 0x60)
CRATE_WOOD_DARK = (0x60, 0x38, 0x18)
CRATE_RIBBON    = (0xE0, 0x30, 0x50)
CRATE_CRACK     = (0x30, 0x1C, 0x0C)


def draw_crate_face(draw, w, h, damage, nof_hits_left):
    """Het kistje, met damage van 0 (gaaf) tot 1 (bijna stuk).

    nof_hits_left komt als cijfer op het kistje te staan. De scheuren zeggen het
    ook al, maar niet precies genoeg om op te spelen - je wilt kunnen zien of er
    nog twee of nog drie treffers nodig zijn, niet ongeveer hoe stuk het is.
    """

    draw.rectangle([0, 0, w - 1, h - 1], fill=CRATE_WOOD)

    # Twee horizontale planken met een naad ertussen, en een lichte bovenrand:
    # zonder die geleding is het een bruin vierkant.
    draw.rectangle([0, 0, w - 1, h // 8], fill=CRATE_WOOD_LIT)
    draw.line([(0, h // 2), (w - 1, h // 2)], fill=CRATE_WOOD_DARK, width=SS)
    draw.rectangle([0, 0, w - 1, h - 1], outline=CRATE_WOOD_DARK, width=2 * SS)

    # Het lint maakt er een cadeau van in plaats van een krat.
    ribbonWidth = max(SS, w // 9)
    draw.rectangle([w // 2 - ribbonWidth // 2, 0, w // 2 + ribbonWidth // 2, h - 1], fill=CRATE_RIBBON)
    draw.rectangle([0, h // 2 - ribbonWidth // 2, w - 1, h // 2 + ribbonWidth // 2], fill=CRATE_RIBBON)

    # De schade. De scheuren staan vast per stadium en zijn niet willekeurig:
    # het plaatje wordt in git bewaard, dus het hoort er elke keer hetzelfde uit
    # te zien.
    cracks = [
        [(0.18, 0.12), (0.32, 0.34), (0.24, 0.52)],
        [(0.78, 0.16), (0.66, 0.38), (0.80, 0.60)],
        [(0.20, 0.70), (0.38, 0.82), (0.30, 0.94)],
        [(0.60, 0.66), (0.74, 0.84), (0.62, 0.96)],
    ]

    nofCracks = int(round(damage * len(cracks)))
    for crack in cracks[:nofCracks]:
        points = [(x * w, y * h) for (x, y) in crack]
        draw.line(points, fill=CRATE_CRACK, width=max(SS, w // 22), joint="curve")

    # Het aantal treffers dat er nog nodig is, midden op het kistje. Met een
    # donkere rand eromheen, want wit op lichthout leest anders niet.
    text = "%d" % nof_hits_left
    font = font_that_fits(text, int(w * 0.5), int(h * 0.52))
    draw_centred_text(draw, (0, 0, w, h), text, font, WHITE,
                      stroke_width=max(1, SS // 2), stroke_fill=CRATE_WOOD_DARK)


def make_present_crate(size, nof_states):
    cells = []
    for i in range(nof_states):
        damage = i / (nof_states - 1) if nof_states > 1 else 0.0

        # Cel 0 is gaaf en toont het volle aantal; elke treffer schuift een cel
        # op. De display rekent dat om met (nofHits - nofHitsLeft).
        nofHitsLeft = nof_states - i

        def colour(img, w, h, damage=damage, nofHitsLeft=nofHitsLeft):
            draw_crate_face(ImageDraw.Draw(img), w, h, damage, nofHitsLeft)

        def mask(img, w, h):
            ImageDraw.Draw(img).rectangle([0, 0, w - 1, h - 1], fill=255)

        cells.append(antialiased(size, size, CRATE_WOOD, colour, mask))

    sheet = Image.new("RGBA", (size * len(cells), size), (0, 0, 0, 0))
    for i, cell in enumerate(cells):
        sheet.paste(cell, (i * size, 0))

    # Zonder alfakanaal wegschrijven: het kistje is vierkant en dekt zijn hele
    # cel, dus er valt niets door te laten. Een alfakanaal dat overal 255 is,
    # levert alleen een waarschuwing van de converter op ("heeft een alpha-kanaal
    # maar geen -trcolor") en geen enkel voordeel. Opaak blitten is bovendien een
    # memcpy per beeldregel in plaats van een per zichtbaar stuk.
    save(sheet.convert("RGB"), "presentCrate-ssh_1x%d.png" % len(cells))


# --- de peddel -------------------------------------------------------------

def make_paddle(width, height):
    radius = height * SS // 2

    def colour(img, w, h):
        draw = ImageDraw.Draw(img)
        for y in range(h):
            t = y / (h - 1)
            draw.line([(0, y), (w - 1, y)], fill=lerp(CYAN, CYAN_DARK, t ** 0.8))
        # Een lichtstreep over de bovenkant - dat maakt er een balk van in
        # plaats van een vlek.
        draw.rounded_rectangle([SS, SS, w - 1 - SS, h // 3],
                               radius=radius // 2, fill=WHITE)

    def mask(img, w, h):
        ImageDraw.Draw(img).rounded_rectangle([0, 0, w - 1, h - 1], radius=radius, fill=255)

    save(antialiased(width, height, CYAN_MID, colour, mask), "paddle-trcolor_FF00FF.png")


# --- de knoppen ------------------------------------------------------------
#
# De PLAY-knop en de twee peddelknoppen delen hun uiterlijk. Dat is geen
# zuinigheid maar de bedoeling: het zijn de enige drie dingen op het scherm die
# je aan kunt raken, en dat hoor je te zien zonder erover na te denken.

BUTTON_RADIUS = 12


def button_face(draw, w, h, radius):
    """Verloop, glans en donkere rand - het rode knoplichaam zonder opschrift."""

    for y in range(h):
        draw.line([(0, y), (w - 1, y)], fill=lerp(RED, RED_DARK, (y / (h - 1)) ** 0.8))

    # Glans over de bovenste helft, net als bij de peddel: dat maakt er een knop
    # van die je in kunt drukken in plaats van een rode rechthoek.
    draw.rounded_rectangle([2 * SS, 2 * SS, w - 1 - 2 * SS, h // 2],
                           radius=radius // 2, fill=lerp(RED_LIT, RED, 0.45))
    draw.rounded_rectangle([0, 0, w - 1, h - 1], radius=radius,
                           outline=RED_DARK, width=2 * SS)


def make_button(width, height, name, draw_label):
    radius = BUTTON_RADIUS * SS

    def colour(img, w, h):
        draw = ImageDraw.Draw(img)
        button_face(draw, w, h, radius)
        draw_label(draw, w, h)

    def mask(img, w, h):
        ImageDraw.Draw(img).rounded_rectangle([0, 0, w - 1, h - 1], radius=radius, fill=255)

    save(antialiased(width, height, RED, colour, mask), name)


def make_play_button(width, height):
    def label(draw, w, h):
        font = font_that_fits("PLAY", int(w * 0.55), int(h * 0.5))
        draw_centred_text(draw, (0, 0, w, h), "PLAY", font, WHITE,
                          stroke_width=SS, stroke_fill=RED_DARK)

    make_button(width, height, "buttonPlay-trcolor_FF00FF.png", label)


def make_paddle_buttons(width, height):
    """De twee peddelknoppen: een massieve driehoek in plaats van "<-" en "->".

    Een pijl uit het font viel bij deze knopmaat weg tegen de rode ondergrond -
    twee dunne streepjes op 80 x 46 pixels. Een driehoek vult de knop en is van
    een armlengte afstand nog te zien, en dat is precies de afstand waarop je
    hem gebruikt.
    """

    def triangle(pointing_left):
        def label(draw, w, h):
            half = int(h * 0.28)
            depth = int(w * 0.22)
            cx, cy = w // 2, h // 2
            if pointing_left:
                points = [(cx - depth, cy), (cx + depth, cy - half), (cx + depth, cy + half)]
            else:
                points = [(cx + depth, cy), (cx - depth, cy - half), (cx - depth, cy + half)]
            draw.polygon(points, fill=WHITE, outline=RED_DARK, width=SS)
        return label

    make_button(width, height, "buttonPaddleLeft-trcolor_FF00FF.png", triangle(True))
    make_button(width, height, "buttonPaddleRight-trcolor_FF00FF.png", triangle(False))


# --- game over -------------------------------------------------------------

def make_game_over(width, height):
    radius = 14 * SS

    def colour(img, w, h):
        draw = ImageDraw.Draw(img)
        draw.rectangle([0, 0, w - 1, h - 1], fill=BOX_DARK)
        draw.rounded_rectangle([0, 0, w - 1, h - 1], radius=radius,
                               outline=GAMEOVER_RED, width=2 * SS)

        font = font_that_fits("GAME OVER", int(w * 0.82), int(h * 0.46))
        draw_centred_text(draw, (0, 0, w, h), "GAME OVER", font, GAMEOVER_RED,
                          stroke_width=SS // 2, stroke_fill=RED_DARK)

    def mask(img, w, h):
        ImageDraw.Draw(img).rounded_rectangle([0, 0, w - 1, h - 1], radius=radius, fill=255)

    save(antialiased(width, height, BOX_DARK, colour, mask), "gameOver-trcolor_FF00FF.png")


def main():
    # Zie de kopregel: deze maten horen bij de constanten in
    # pdga_GameDisplayControl.h en pdga_StartPageDisplayControl.h.
    playfield_width  = 320
    playfield_height = 180
    wall_thickness   = 10
    ball_cell_size   = 28	# de spritesheet-cel; de bal erin blijft 12 (BallSizePix)
    nof_burning      = 5
    paddle_width     = 50
    paddle_height    = 12
    goal_line_height = 2
    button_width     = 120
    button_height    = 54
    game_over_width  = 256
    game_over_height = 64

    # De present crate: even groot als een cel van de bal-sheet, en vijf cellen
    # want hij begint met vijf treffers tegoed (UC07).
    crate_size       = 28
    nof_crate_states = 5

    # De peddelknoppen stonden op 250 x 190 promille van 320 x 240. In pixels is
    # dat 80 x 45,6 - en juist die halve pixel is de reden dat ze nu in pixels
    # staan: zie de toelichting in pdga_PlayButton.h.
    paddle_button_width  = 80
    paddle_button_height = 46

    print("pong_cleangui_dg_adv images:")
    make_walls(playfield_width, playfield_height, wall_thickness)
    make_goal_line(playfield_width, goal_line_height)
    make_ball_sheet(ball_cell_size, nof_burning)
    make_paddle(paddle_width, paddle_height)
    make_play_button(button_width, button_height)
    make_paddle_buttons(paddle_button_width, paddle_button_height)
    make_present_crate(crate_size, nof_crate_states)
    make_game_over(game_over_width, game_over_height)


if __name__ == "__main__":
    main()
