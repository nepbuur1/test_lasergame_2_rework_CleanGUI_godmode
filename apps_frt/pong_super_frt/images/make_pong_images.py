#!/usr/bin/env python3
"""Tekent de bron-PNG's van pong_super.

De PNG's staan in git en zijn het echte bronmateriaal - dit script is er om ze
te kunnen bijstellen zonder een tekenprogramma, en om vast te leggen waar de
maten vandaan komen. Vervang gerust een PNG door handwerk; zolang de afmeting
klopt merkt de app er niets van.

    python3 apps/pong_super/images/make_pong_images.py
    python3 tools/img2rgb565.py --pal8 apps/pong_super/images

De maten zijn geen smaak maar volgen uit psupf_GameDisplayControl.h: scherm
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

# Smal en technisch, voor het regeltje onder de titel. Ruime letterafstand doet
# de rest - dat is wat een sci-fi-tussenkop vooral onderscheidt, meer dan de
# lettervorm zelf.
FONT_PATH_CONDENSED = "/usr/share/fonts/truetype/ubuntu/Ubuntu-C.ttf"

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


def ball_cell(size, phase, scale=1.0):
    """Een cel van de spritesheet. phase is None voor de gewone bal.

    scale krimpt de bal en zijn vlammen binnen dezelfde cel. De cel blijft even
    groot - daar hangt de panelmaat aan - en de bal blijft in het midden staan,
    zodat het spel hem op dezelfde plek blijft plaatsen terwijl hij wegbrandt.
    """

    ballDiameter = 12 * scale		# BallSizePix; de botsingsmaat verandert niet mee.

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


def make_ball_sheet(cell_size, nof_burning, burn_scales):
    """Cel 0 is de gewone bal; daarna per stadium een cyclus van nof_burning.

    De stadia horen bij de drie seconden waarin een bal opbrandt (UC05 stap 1d):
    een seconde op volle grootte, een seconde half, een seconde een kwart. Een bal
    die onder de doellijn door valt is binnen een seconde weg en komt dus nooit
    verder dan het eerste stadium - dat hoeft nergens apart te worden geregeld.
    """

    cells = [ball_cell(cell_size, None)]
    for scale in burn_scales:
        for i in range(nof_burning):
            cells.append(ball_cell(cell_size, i / nof_burning, scale))

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
    save(sheet.convert("RGB"), "crate-ssh_1x%d.png" % len(cells))


# --- de fakkels ------------------------------------------------------------
#
# Vijf cellen die cyclisch worden afgespeeld, net als bij de vuurkorf. Ze staan
# links en rechts van de PLAY-knop op de startpagina en van PLAY AGAIN op de
# hall of fame.

TORCH_WOOD      = (0x8C, 0x5A, 0x2C)
TORCH_WOOD_DARK = (0x4A, 0x2C, 0x12)
TORCH_BAND      = (0x70, 0x70, 0x78)


def draw_torch(draw, w, h, phase, colour_layer):
    """Een vlam in twee lagen: een lichaam en de tongen erboven.

    De eerste versie was een enkele bundel die met de fase naar links en rechts
    zwiepte. Dat leest als een wapperend vlaggetje en niet als vuur: bij echt
    vuur staan de tongen vrijwel rechtop en is het de HOOGTE die pulseert, elk
    met een eigen fase.

    Er zaten ook losgelaten vlammetjes boven de vlam. Die zijn eruit: als enkele
    ellips bleven het op deze maat herkenbare rondjes, en een rondje boven een
    vuur leest niet als een vonk maar als een bolletje.
    """

    # De kop zit op 58% van de hoogte en niet halverwege: het vuur heeft meer
    # ruimte nodig dan de steel, en wat erboven uitkomt wordt door de celrand
    # afgesneden - dat zag je als een vlakke bovenkant. Alles hieronder is zo
    # gekozen dat de hoogste tong en het bovenste vlammetje binnen de cel vallen.
    cx = w // 2
    handleTop = int(h * 0.58)
    handleHalfW = max(SS, int(w * 0.11))

    bodyHalfW = w * 0.26
    bodyTop = handleTop - h * 0.13

    def blob(x, y, r, t):
        if r < 0.35 * SS:
            return
        draw.ellipse([x - r, y - r, x + r, y + r],
                     fill=flame_colour(t) if colour_layer else 255)

    # 1. Het lichaam: een stapel ellipsen die naar boven versmalt. Dat geeft de
    #    vlam een voet in plaats van een punt die uit het niets begint.
    steps = 12
    for i in range(steps - 1, -1, -1):
        t = i / (steps - 1)
        rx = bodyHalfW * (1.0 - 0.55 * t)
        ry = (handleTop - bodyTop) * 0.55 * (1.0 - 0.25 * t) + 0.4 * SS
        yc = handleTop - (handleTop - bodyTop) * t * 0.85

        # De voet niet onder de kop laten uitkomen. De onderste ellips is ruim twee
        # keer zo breed als de steel (0.26w tegen 0.11w) en heeft zijn middelpunt op
        # handleTop, dus zijn onderste helft stak eronder uit. De steel dekte daar
        # alleen het midden van af, en wat er links en rechts naast bleef staan las
        # als een lichtgele boog - het enige aan deze fakkel dat er niet uitzag als
        # vuur. Afplatten kan niet opvallen: de band loopt door tot
        # handleTop + h*0.04 en wordt hierna getekend, dus die dekt de rechte
        # onderrand af.
        yBottom = min(yc + ry, handleTop)

        draw.ellipse([cx - rx, yc - ry, cx + rx, yBottom],
                     fill=flame_colour(t * 0.45) if colour_layer else 255)

    # 2. De tongen: bijna rechtop, elk met een eigen fase zodat ze om beurten
    #    oplaaien. De x-variatie is klein en met opzet - meer wordt gezwiep.
    # Vijf smalle tongen in plaats van vier brede: bij brede tongen loopt alles
    # in elkaar over en zie je weer een enkele bundel.
    tongues = [(-0.52, 0.58), (-0.26, 0.86), (0.0, 1.0), (0.26, 0.90), (0.52, 0.62)]
    for k, (offset, heightScale) in enumerate(tongues):
        pulse = 0.50 + 0.50 * (0.5 + 0.5 * math.sin(2.0 * math.pi * (phase + k / len(tongues))))
        height = (h * 0.30) * heightScale * pulse

        steps = 16
        for sIdx in range(steps - 1, -1, -1):
            t = sIdx / (steps - 1)
            # Twee soorten zijwaartse beweging, allebei klein: een eigen slinger
            # per tong, en een lichte helling van de vlam als geheel. Meer dan dit
            # en het wordt weer het wapperende vlaggetje van de eerste poging.
            lean = math.sin(2.0 * math.pi * phase) * w * 0.045 * t
            x = cx + offset * bodyHalfW * (1.0 - 0.55 * t) \
                + math.sin(t * 2.2 + 2.0 * math.pi * (phase + k * 0.13)) * w * 0.045 * t \
                + lean
            y = bodyTop - height * t
            r = w * 0.082 * (1.0 - t) ** 0.5 + 0.30 * SS
            blob(x, y, r, 0.22 + 0.78 * t)

    # De steel: iets taps toelopend, met een band onder de kop.
    handle = [(cx - handleHalfW, handleTop),
              (cx + handleHalfW, handleTop),
              (cx + int(handleHalfW * 0.7), h - 1),
              (cx - int(handleHalfW * 0.7), h - 1)]

    if colour_layer:
        draw.polygon(handle, fill=TORCH_WOOD, outline=TORCH_WOOD_DARK)
        draw.rectangle([cx - handleHalfW - SS, handleTop - int(h * 0.04),
                        cx + handleHalfW + SS, handleTop + int(h * 0.04)],
                       fill=TORCH_BAND, outline=TORCH_WOOD_DARK)
    else:
        draw.polygon(handle, fill=255)
        draw.rectangle([cx - handleHalfW - SS, handleTop - int(h * 0.04),
                        cx + handleHalfW + SS, handleTop + int(h * 0.04)], fill=255)


def make_torch(width, height, nof_states):
    cells = []
    for i in range(nof_states):
        phase = i / nof_states

        def colour(img, w, h, phase=phase):
            draw_torch(ImageDraw.Draw(img), w, h, phase, True)

        def mask(img, w, h, phase=phase):
            draw_torch(ImageDraw.Draw(img), w, h, phase, False)

        cells.append(antialiased(width, height, FLAME_MID, colour, mask))

    sheet = Image.new("RGBA", (width * len(cells), height), (0, 0, 0, 0))
    for i, cell in enumerate(cells):
        sheet.paste(cell, (i * width, 0))

    save(sheet, "torch-trcolor_FF00FF-ssh_1x%d.png" % len(cells))


# --- de cijfers van de score -----------------------------------------------
#
# Tien cellen, 0 tot en met 9. De score bestaat uit vier panels naast elkaar en
# elk panel kiest zijn cel; er wordt dus geen tekst gerenderd. Dat scheelt niet
# alleen tekentijd, het houdt de cijfers ook stabiel van breedte - een
# proportioneel font laat de score wiebelen zodra er een 1 in staat.

def make_digits(width, height):
    def cell(digit):
        def colour(img, w, h):
            draw = ImageDraw.Draw(img)
            font = font_that_fits("8", int(w * 0.78), int(h * 0.80))
            draw_centred_text(draw, (0, 0, w, h), "%d" % digit, font, CYAN,
                              stroke_width=max(1, SS // 2), stroke_fill=CYAN_DARK)

        def mask(img, w, h):
            draw = ImageDraw.Draw(img)
            font = font_that_fits("8", int(w * 0.78), int(h * 0.80))
            draw_centred_text(draw, (0, 0, w, h), "%d" % digit, font, 255,
                              stroke_width=max(1, SS // 2), stroke_fill=255)

        return antialiased(width, height, CYAN, colour, mask)

    cells = [cell(digit) for digit in range(10)]

    sheet = Image.new("RGBA", (width * len(cells), height), (0, 0, 0, 0))
    for i, c in enumerate(cells):
        sheet.paste(c, (i * width, 0))

    save(sheet, "scoreDigits-trcolor_FF00FF-ssh_1x%d.png" % len(cells))


# --- de vuurkorf -----------------------------------------------------------
#
# Vijf cellen die cyclisch worden afgespeeld (UC08). Anders dan bij de crate is de
# cel hier geen teller maar een animatiefase: de display schuift er per beeld een
# op, zodat het vuur doorbrandt zolang de korf er staat.

BIN_METAL      = (0x50, 0x50, 0x58)
BIN_METAL_LIT  = (0x88, 0x88, 0x94)
BIN_METAL_DARK = (0x24, 0x24, 0x28)
BIN_EMBER      = (0xFF, 0x60, 0x10)


def draw_fire_bin(draw, w, h, phase, colour_layer):
    """De korf onderin, de vlammen erboven.

    De korf staat op de onderste 40% en de vlammen komen eruit. Zo blijft het
    middelpunt van de cel - waar het spel het object plaatst - in het vuur zelf
    liggen, en niet ergens in de lucht erboven.
    """

    binTop    = int(h * 0.60)
    binBottom = h - 1
    binHalfW  = int(w * 0.34)
    cx        = w // 2

    # De vlammen eerst, zodat de korf er voor staat en het vuur er echt uit lijkt
    # te komen in plaats van erachter te zweven.
    tongues = [(-0.55, 0.68), (-0.25, 0.92), (0.05, 1.0), (0.34, 0.86), (0.60, 0.62)]

    for k, (offset, heightScale) in enumerate(tongues):
        wobble = math.sin(2.0 * math.pi * (phase + k / len(tongues)))
        height = binTop * 0.92 * heightScale * (0.68 + 0.32 * wobble)

        steps = 18
        for s in range(steps - 1, -1, -1):
            t = s / (steps - 1)
            x = cx + offset * binHalfW * (1.0 - 0.30 * t) \
                + math.sin(t * 3.2 + 2.0 * math.pi * phase) * binHalfW * 0.26 * t
            y = binTop - height * t
            r = binHalfW * 0.38 * (1.0 - t) ** 0.7 + 0.5 * SS

            box = [x - r, y - r, x + r, y + r]
            draw.ellipse(box, fill=flame_colour(t) if colour_layer else 255)

    # De korf: een trapezium dat naar onderen versmalt, met een lichte rand
    # bovenaan en gloeiende kolen erin.
    top = [(cx - binHalfW, binTop), (cx + binHalfW, binTop),
           (cx + int(binHalfW * 0.62), binBottom), (cx - int(binHalfW * 0.62), binBottom)]

    if colour_layer:
        draw.polygon(top, fill=BIN_METAL, outline=BIN_METAL_DARK)
        draw.line([(cx - binHalfW, binTop), (cx + binHalfW, binTop)],
                  fill=BIN_METAL_LIT, width=2 * SS)
        draw.line([(cx - int(binHalfW * 0.8), binTop + int(h * 0.06)),
                   (cx + int(binHalfW * 0.8), binTop + int(h * 0.06))],
                  fill=BIN_EMBER, width=2 * SS)

        # Twee spijlen, zodat het een korf is en geen emmer.
        for t in (-0.35, 0.35):
            draw.line([(cx + int(binHalfW * t), binTop),
                       (cx + int(binHalfW * t * 0.62), binBottom)],
                      fill=BIN_METAL_DARK, width=SS)
    else:
        draw.polygon(top, fill=255)


def make_fire_bin(size, nof_states):
    cells = []
    for i in range(nof_states):
        phase = i / nof_states

        def colour(img, w, h, phase=phase):
            draw_fire_bin(ImageDraw.Draw(img), w, h, phase, True)

        def mask(img, w, h, phase=phase):
            draw_fire_bin(ImageDraw.Draw(img), w, h, phase, False)

        cells.append(antialiased(size, size, FLAME_MID, colour, mask))

    sheet = Image.new("RGBA", (size * len(cells), size), (0, 0, 0, 0))
    for i, cell in enumerate(cells):
        sheet.paste(cell, (i * size, 0))

    save(sheet, "fireBin-trcolor_FF00FF-ssh_1x%d.png" % len(cells))


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


def make_text_button(width, height, text, name):
    def label(draw, w, h):
        font = font_that_fits(text, int(w * 0.72), int(h * 0.5))
        draw_centred_text(draw, (0, 0, w, h), text, font, WHITE,
                          stroke_width=SS, stroke_fill=RED_DARK)

    make_button(width, height, name, label)


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


# --- het regeltje onder de titel -------------------------------------------

def draw_tracked_text(draw, text, font, x, y, fill, tracking):
    """Tekst met extra ruimte tussen de letters.

    PIL kent geen letterafstand, dus teken per letter en schuif zelf op. De
    breedte per letter komt uit de font-metriek en niet uit de bounding box: die
    laatste laat spaties op nul uitkomen, en dan plakken de woorden aan elkaar.
    """

    for ch in text:
        draw.text((x, y), ch, font=font, fill=fill)
        x += font.getlength(ch) + tracking


def tracked_text_width(text, font, tracking):
    return sum(font.getlength(ch) + tracking for ch in text) - tracking


def make_credits(width, height, text, name):
    def draw_credits(draw, w, h, colour_layer):
        # De grootste maat die met de letterafstand erbij nog past.
        tracking = max(1, w // 120)
        for size in range(h, 4, -1):
            font = ImageFont.truetype(FONT_PATH_CONDENSED, size)
            if tracked_text_width(text, font, tracking) <= w * 0.96:
                break

        textWidth = tracked_text_width(text, font, tracking)
        left, top, right, bottom = font.getbbox(text)
        x = (w - textWidth) / 2.0
        y = (h - (bottom - top)) / 2.0 - top

        draw_tracked_text(draw, text, font, x, y,
                          CYAN_MID if colour_layer else 255, tracking)

    def colour(img, w, h):
        draw_credits(ImageDraw.Draw(img), w, h, True)

    def mask(img, w, h):
        draw_credits(ImageDraw.Draw(img), w, h, False)

    save(antialiased(width, height, CYAN_MID, colour, mask), name)


# --- titels ----------------------------------------------------------------
#
# "SUPER PONG" op de startpagina (UC02 stap 1) en "HALL OF FAME" boven de lijst
# (UC03 stap 2). Allebei dezelfde vorm: dikke letters met een schuine slagschaduw
# en een omlijning, zodat ze op de zwarte achtergrond staan te staan.

TITLE_FILL   = (0x00, 0xFF, 0xFF)
TITLE_SHADOW = (0x00, 0x50, 0x70)
TITLE_EDGE   = (0x00, 0x18, 0x28)


def make_title(width, height, text, name, fill=TITLE_FILL):
    def draw_title(draw, w, h, colour_layer):
        font = font_that_fits(text, int(w * 0.94), int(h * 0.72))

        # De schaduw eerst, twee pixels naar rechtsonder. In de masker-laag hoort
        # hij mee: anders valt hij buiten de vorm en wordt hij weggeknipt.
        # Twee ECHTE pixels, dus 2 * SS in het vergrote vlak. Rekenen met w hier
        # gaat mis: dat is de supersample-breedte, en dan wordt de schaduw vier
        # keer zo ver verschoven als bedoeld - een tweede regel tekst in plaats
        # van een schaduw.
        offset = 2 * SS
        draw_centred_text(draw, (offset, offset, w + offset, h + offset), text, font,
                          TITLE_SHADOW if colour_layer else 255,
                          stroke_width=SS, stroke_fill=TITLE_SHADOW if colour_layer else 255)

        draw_centred_text(draw, (0, 0, w, h), text, font,
                          fill if colour_layer else 255,
                          stroke_width=SS, stroke_fill=TITLE_EDGE if colour_layer else 255)

    def colour(img, w, h):
        draw_title(ImageDraw.Draw(img), w, h, True)

    def mask(img, w, h):
        draw_title(ImageDraw.Draw(img), w, h, False)

    save(antialiased(width, height, fill, colour, mask), name)


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
    # psupf_GameDisplayControl.h en psupf_StartPageDisplayControl.h.
    playfield_width  = 320
    playfield_height = 180
    wall_thickness   = 10
    ball_cell_size   = 28	# de spritesheet-cel; de bal erin blijft 12 (BallSizePix)
    nof_burning      = 5
    burn_scales      = [1.0, 0.5, 0.25]		# een stadium per seconde opbranden
    paddle_width     = 50
    paddle_height    = 12
    goal_line_height = 2
    button_width     = 120
    button_height    = 54

    # "PLAY AGAIN" is breder dan "PLAY" en krijgt daarom een eigen maat; de
    # letters blijven dan even groot als op de startpagina.
    play_again_width = 200
    game_over_width  = 256
    game_over_height = 64

    # De present crate: even groot als een cel van de bal-sheet, en vijf cellen
    # want hij begint met vijf treffers tegoed (UC07).
    crate_size       = 28
    nof_crate_states = 5

    # De vuurkorf: even groot als de crate, vijf cellen die cyclisch worden
    # afgespeeld (UC08).
    fire_bin_size       = 28
    nof_fire_bin_states = 5

    # De score: vier cijfers tussen de peddelknoppen. Die staan op x 6..86 en
    # 234..314, dus er is 148 pixel tussenruimte; vier van 28 is 112 en dat past
    # met marge. De hoogte blijft binnen de 46 van de knoppen.
    score_digit_width  = 28
    score_digit_height = 36

    # De peddelknoppen stonden op 250 x 190 promille van 320 x 240. In pixels is
    # dat 80 x 45,6 - en juist die halve pixel is de reden dat ze nu in pixels
    # staan: zie de toelichting in psupf_PlayButton.h.
    paddle_button_width  = 80
    paddle_button_height = 46

    print("pong_super images:")
    make_walls(playfield_width, playfield_height, wall_thickness)
    make_goal_line(playfield_width, goal_line_height)
    make_ball_sheet(ball_cell_size, nof_burning, burn_scales)
    make_paddle(paddle_width, paddle_height)
    make_text_button(button_width, button_height, "PLAY", "buttonPlay-trcolor_FF00FF.png")
    make_text_button(play_again_width, button_height, "PLAY AGAIN",
                     "buttonPlayAgain-trcolor_FF00FF.png")
    make_paddle_buttons(paddle_button_width, paddle_button_height)
    make_present_crate(crate_size, nof_crate_states)
    make_fire_bin(fire_bin_size, nof_fire_bin_states)
    make_digits(score_digit_width, score_digit_height)

    # De fakkels naast de knoppen. 72 hoog past nog net naast de PLAY
    # AGAIN-knop op de hall of fame, die tot y=230 loopt.
    # Tien frames in plaats van vijf: de tongen laaien vloeiender op en de
    # vlammetjes leggen hun weg naar boven in kleinere stappen af.
    make_torch(36, 72, 10)
    make_game_over(game_over_width, game_over_height)
    make_title(288, 72, "SUPER PONG", "titleSuperPong-trcolor_FF00FF.png")
    make_title(272, 44, "HALL OF FAME", "titleHallOfFame-trcolor_FF00FF.png")
    make_credits(288, 20, "by Marius Versteegen, 2026",
                 "creditsLine-trcolor_FF00FF.png")


if __name__ == "__main__":
    main()
