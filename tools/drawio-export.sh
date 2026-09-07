#!/usr/bin/env bash
#
# Exporteert één pagina uit een .drawio-bestand naar een plaatje, door vanuit WSL
# de draw.io desktop-app op de Windows-kant aan te roepen. Nut: een diagram is
# zonder renderen niet te controleren, en de losse .drawio is platte XML die je
# wél met tekstgereedschap kunt bewerken maar niet kunt bekíjken.
#
#   tools/drawio-export.sh <bestand.drawio> --list
#   tools/drawio-export.sh <bestand.drawio> <paginanaam> [uitvoer.png] [schaal]
#
# Het formaat volgt de extensie van de uitvoernaam (png, svg, pdf, jpg).
# Schaal geldt alleen voor bitmapformaten; 2 is een prettige leesbare default.
#
set -euo pipefail

die() { echo "drawio-export: $*" >&2; exit 1; }

# --- draw.io desktop opzoeken -----------------------------------------------
# Niet gokken op één pad: de installatie kan per machine en per gebruiker
# verschillen. DRAWIO in de omgeving wint altijd.
find_drawio() {
    if [ -n "${DRAWIO:-}" ]; then
        [ -x "$DRAWIO" ] || die "DRAWIO=$DRAWIO is niet uitvoerbaar"
        echo "$DRAWIO"; return
    fi
    local c
    for c in "/mnt/c/Program Files/draw.io/draw.io.exe" \
             "/mnt/c/Program Files (x86)/draw.io/draw.io.exe" \
             "$(command -v drawio 2>/dev/null || true)"; do
        [ -n "$c" ] && [ -x "$c" ] && { echo "$c"; return; }
    done
    # Nog niet gevonden: per-gebruiker-installatie onder AppData.
    for c in /mnt/c/Users/*/AppData/Local/Programs/draw.io/draw.io.exe; do
        [ -x "$c" ] && { echo "$c"; return; }
    done
    die "draw.io niet gevonden. Zet DRAWIO=/pad/naar/draw.io.exe in de omgeving."
}

# --- paginanamen in bestandsvolgorde ----------------------------------------
# De attribuutvolgorde binnen <diagram> wisselt per pagina (soms name vóór id,
# soms erna), dus eerst de hele tag pakken en pas daarna het name-attribuut.
page_names() {
    grep -o '<diagram [^>]*>' "$1" | sed -n 's/.*name="\([^"]*\)".*/\1/p'
}

[ $# -ge 1 ] || die "gebruik: $(basename "$0") <bestand.drawio> <paginanaam|--list> [uitvoer] [schaal]"
FILE="$1"
[ -f "$FILE" ] || die "bestand bestaat niet: $FILE"

# Een gecomprimeerde .drawio heeft base64-brij in plaats van <mxCell>-tags.
# Renderen lukt dan nog wel, maar bewerken met tekstgereedschap niet; meld het.
if ! grep -q '<mxCell' "$FILE"; then
    echo "drawio-export: let op, $FILE lijkt gecomprimeerd opgeslagen." >&2
    echo "  Zet in draw.io uit: Bestand > Eigenschappen > Gecomprimeerd." >&2
fi

PAGE="${2:---list}"
if [ "$PAGE" = "--list" ]; then
    page_names "$FILE" | nl -ba
    exit 0
fi

# grep -n -x -F: exacte regelmatch, geen regex-interpretatie van de paginanaam.
# De '|| true' is nodig: zonder match geeft grep 1, en dan zou set -e samen met
# pipefail hier meteen afbreken — zonder de uitleg hieronder.
IDX=$(page_names "$FILE" | grep -nxF "$PAGE" | cut -d: -f1 | head -1 || true)
[ -n "$IDX" ] || die "pagina '$PAGE' niet gevonden. '--list' geeft de lijst."

OUT="${3:-${PAGE// /_}.png}"
SCALE="${4:-2}"
FMT="${OUT##*.}"
[ "$FMT" != "$OUT" ] || die "uitvoernaam '$OUT' heeft geen extensie; gebruik bv. .png of .svg"

OUTDIR=$(dirname "$OUT")
[ -d "$OUTDIR" ] || die "uitvoermap bestaat niet: $OUTDIR"

# draw.io.exe is een Windows-programma: het snapt geen /home/... maar wel het
# \\wsl.localhost\...-pad dat wslpath ervan maakt.
#
# --page-index is 1-gebaseerd, ondanks de naam: index 0 levert stilzwijgend niets
# op. Dat komt goed uit, want de regelnummers van grep -n zijn dat ook.
#
# Electron kwettert op stdout ("bron -> doel") en strooit GPU-cachefouten over
# stderr, allebei ook als alles goed gaat. Opvangen dus, en alleen tonen als het
# écht misging — anders is de uitvoer niet te lezen.
LOG=$(mktemp)
trap 'rm -f "$LOG"' EXIT
"$(find_drawio)" --export --format "$FMT" --scale "$SCALE" \
    --page-index "$IDX" \
    --output "$(wslpath -w "$OUT")" \
    "$(wslpath -w "$FILE")" >"$LOG" 2>&1 || true

if [ ! -s "$OUT" ]; then
    sed 's/^/  /' "$LOG" >&2
    die "export leverde niets op. Staat het bestand open in draw.io?"
fi
echo "$OUT ($(stat -c%s "$OUT") bytes) — pagina $IDX: $PAGE"
