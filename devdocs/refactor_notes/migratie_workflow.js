export const meta = {
  name: 'cleangui-migreer-bestanden',
  description: 'Migreer een lijst bestanden naar de nieuwe show/enable/draw-API, met tegensprekende verificatie per bestand',
  phases: [
    { title: 'Migreren', detail: 'een agent per bestand' },
    { title: 'Verifieren', detail: 'een tegenspreker per bestand' },
  ],
}

const REPO = '/home/marius/dev/esp54/test_lasergame_2_rework_CleanGUI_godmode'

const API = `
DE NIEUWE API (staat er al in, libs/CleanGUI/src/internals/crt_Widget.h):

  makeDrawable(bIncludeChildren)     // alleen de vlag _bDrawable=true. Tekent NIETS.
  makeNonDrawable(bIncludeChildren)  // alleen de vlag _bDrawable=false. Wist NIETS.
  redraw(bIncludeChildren)           // teken NU. Raakt geen enkele vlag aan.
  show(bIncludeChildren)             // == makeDrawable + redraw
  hide(bIncludeChildren)             // == makeNonDrawable + repaintRegion(eigen rect)
  enable(bIncludeChildren)           // aanraakbaarheid
  disable(bIncludeChildren)
  isDrawable(), isEnabled()

WEG, dus de compiler wijst deze aan:
  setDrawable(bool, bool)            -> makeDrawable(b) / makeNonDrawable(b)
  isShown() en _bVisible             -> houd zelf een bool bij als je het echt nodig hebt
  Text::redraw()                     -> redraw(false)
  Panel::show/hide en Text::show/hide overrides bestaan niet meer (Widget doet het)

DE VIER VERTAALPATRONEN — dit is waar het misgaat als je niet oplet:

1. show() ENABLET NIET MEER, hide() DISABELT NIET MEER.
   De compiler zegt hier NIETS over. Elke plek die op dat neveneffect leunde
   moet een eigen enable()/disable() krijgen. Staat die er al (vandaag als
   "dubbelop"), dan MOET hij blijven staan - hij wordt juist dragend. Haal hem
   dus NOOIT weg als vermeende dubbeling.

2. show(true) op een boom met bewust uitgezette kinderen is FOUT.
   De nieuwe show() zet _bDrawable op de HELE subboom. Staan er kinderen met
   opzet op niet-tekenbaar (ongebruikte slots, een vignet dat later pas mag),
   dan komen die er allemaal weer bij. Gebruik daar redraw(true): dat tekent
   wat tekenbaar is en laat de vlaggen met rust.

3. setDrawable(false, true); hide(true); disable(true)
   -> makeNonDrawable(true); disable(true)     [de hide() VERVALT]
   Dat drieluik betekende "verberg zonder ook maar iets te tekenen": de
   setDrawable ervoor zorgde dat hide() zijn wisvlak oversloeg. De nieuwe
   hide() tekent juist wel - hij laat de boom vanaf de wortel over dat vlak
   gaan - en dat is precies wat daar niet mag als een andere taak dezelfde
   commandobuffer gebruikt. makeNonDrawable dekt de vlag, disable de rest.

4. Een gewone hide() die WEL moet wissen blijft gewoon hide().
   Dat is nu zelfs beter: er komt terug wat eronder zat in plaats van een
   vulling met de eigen colBg.
`

const files = args

const SCHEMA_MIGRATIE = {
  type: 'object',
  additionalProperties: false,
  required: ['file', 'changes', 'unchanged', 'concerns'],
  properties: {
    file: { type: 'string' },
    changes: {
      type: 'array',
      items: {
        type: 'object',
        additionalProperties: false,
        required: ['line', 'was', 'now', 'why'],
        properties: {
          line: { type: 'integer' },
          was: { type: 'string' },
          now: { type: 'string' },
          why: { type: 'string' },
        },
      },
    },
    unchanged: { type: 'string', description: 'welke call sites bewust ongewijzigd bleven, en waarom' },
    concerns: { type: 'string', description: 'twijfelgevallen; leeg als er geen zijn' },
  },
}

const SCHEMA_VERIFICATIE = {
  type: 'object',
  additionalProperties: false,
  required: ['file', 'verdict', 'problems'],
  properties: {
    file: { type: 'string' },
    verdict: { type: 'string', enum: ['goed', 'twijfel', 'fout'] },
    problems: {
      type: 'array',
      items: {
        type: 'object',
        additionalProperties: false,
        required: ['line', 'problem', 'fix', 'severity'],
        properties: {
          line: { type: 'integer' },
          problem: { type: 'string' },
          fix: { type: 'string' },
          severity: { type: 'string', enum: ['breekt-gedrag', 'breekt-build', 'stijl'] },
        },
      },
    },
  },
}

function briefPath(file) {
  return `devdocs/refactor_notes/per_bestand/${file.replace(/\//g, '__')}.md`
}

const results = await pipeline(
  files,
  (file) => agent(
    `Je migreert EEN bestand in de repo ${REPO} (branch show_drawable_enable_refactoring)
naar de nieuwe CleanGUI-API. De bibliotheek is al om; alleen dit bestand nog.

BESTAND: ${file}

${API}

WERKWIJZE
1. Lees ${file} VOLLEDIG met Read.
2. Lees de migratiebrief ${briefPath(file)}. Die is opgesteld door een
   inventarisatie vooraf. Behandel hem als een goed geinformeerd advies, NIET
   als waarheid: klopt hij niet met wat je in de code ziet, volg dan de code en
   meld het onder concerns.
3. Pas het bestand aan met Edit. Bouw NIET - er is een gedeelde build-map en
   een gedeeld bord, dus idf.py draait alleen de opdrachtgever. Roep geen
   idf.py, geen flash, geen serial_capture aan.
4. Raak geen ANDER bestand aan dan ${file}.

SCHRIJFSTIJL - deze codebase heeft een uitgesproken stijl, houd je eraan:
- Commentaar is Nederlands en legt uit WAAROM iets zo staat, niet wat de regel
  doet. Bestaand commentaar dat na de migratie niet meer klopt, herschrijf je;
  commentaar dat nog klopt laat je met rust.
- Voeg GEEN commentaar toe in de trant van "gemigreerd naar de nieuwe API" of
  "was eerst setDrawable". Dat is jij die tegen de reviewer praat; de volgende
  lezer heeft er niets aan. Alleen een beperking die de code zelf niet toont
  verdient een regel commentaar.
- Als een enable()/disable() van overbodig naar dragend gaat, is dat wel het
  soort ding dat een korte regel verdient - want wie hem later "opruimt" breekt
  de app stilzwijgend.

Rapporteer elke wijziging met regelnummer, wat er stond, wat er nu staat en
waarom. Zeg ook expliciet welke call sites je bewust NIET hebt aangeraakt.`,
    { label: `migreer:${file.split('/').pop()}`, phase: 'Migreren', schema: SCHEMA_MIGRATIE }
  ),
  (migratie, file) => agent(
    `Je bent de tegenspreker. Iemand heeft zojuist ${file} in de repo ${REPO}
gemigreerd naar de nieuwe CleanGUI-API. Jouw taak is FOUTEN VINDEN, niet
bevestigen. Ga er vanuit dat er iets mis is en zoek het.

${API}

WAT DE MIGRATOR ZEGT TE HEBBEN GEDAAN:
${JSON.stringify(migratie, null, 1)}

CONTROLEER, door het bestand zelf te lezen (Read) en met
\`git diff -- ${file}\` te kijken wat er werkelijk is veranderd:

1. Is er ergens een show()/hide() blijven staan waar de app op het ENABLE- of
   DISABLE-neveneffect leunde, zonder dat er een expliciete enable()/disable()
   bij is gekomen? Dit is de gevaarlijkste fout en de compiler zegt er niets
   over. Kijk per pagina: wie zet hem aan, wie zet hem uit, en klopt dat nu nog?
2. Is er een enable()/disable() WEGGEHAALD omdat hij dubbelop leek? Dat is bijna
   altijd fout - juist die regels worden nu dragend.
3. Staat er ergens show(true) op een boom waarin kinderen bewust op
   niet-tekenbaar staan? Dan komen die er weer bij en had er redraw(true) moeten
   staan. Zoek de makeNonDrawable-aanroepen op en kijk of een latere show(true)
   ze ongedaan maakt.
4. Is een "verberg zonder te tekenen"-plek nog steeds tekenvrij? Een
   overgebleven hide() schuift daar tekenwerk de gedeelde commandobuffer in.
5. Zijn er verwijzingen naar isShown(), setDrawable() of Text::redraw() blijven
   staan? Die breken de build.
6. Is er commentaar blijven staan dat nu iets beweert dat niet meer waar is?
7. Is er commentaar TOEGEVOEGD dat alleen maar de wijziging aankondigt
   ("nu makeNonDrawable", "aangepast voor de nieuwe API")? Dat is ruis.

Bouw NIET en wijzig NIETS - je rapporteert alleen. Een leeg problems-veld met
verdict 'goed' is een prima uitkomst, maar zeg dat pas als je echt hebt
gezocht.`,
    { label: `verifieer:${file.split('/').pop()}`, phase: 'Verifieren', schema: SCHEMA_VERIFICATIE }
  )
)

const ok = results.filter(Boolean)
const problemen = ok.filter(r => r.problems && r.problems.length > 0)
log(`${ok.length}/${files.length} bestanden geverifieerd, ${problemen.length} met opmerkingen`)

return ok
