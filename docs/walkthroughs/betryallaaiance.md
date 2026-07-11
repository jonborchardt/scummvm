<!-- claude-index -->
# Claude Index — how to navigate this walkthrough

**Game:** Betrayed Alliance Book 1 (fan-made SCI0 parser game). Not a default `build_and_run.ps1` target — it needs its own ScummVM target configured (`-Game <target>`); SQ3 and QFG1 remain the primary Roger test games. Official hint book link is just below this index.
**File format:** one parser command per line as `command # purpose`; blank lines separate rooms/scenes. Commands are strictly sequential — items and story flags gained earlier are required later, so start from a phase boundary only if a save already covers its prerequisites.
**Driving the game:** feed each command from a `.rin` script as `type "<command>"` then `key ENTER`, with a `wait` between commands; see CLAUDE.md "Autonomous verification loop" for the harness and capture grammar.
**Finding a phase:** grep this file for the Anchor string — each anchor matches exactly one line.

| # | Phase | Where / what happens | Key items & outcomes | Anchor |
|---|---|---|---|---|
| 1 | Wizard room start | Opening room; mirror exposition | map (required to leave), marbles | `look room # wizard room` |
| 2 | Pond & Leah | Meet Bobby and Leah; Julyn rumor | eastern-cave clue | `talk to bobby` |
| 3 | Graveyard | Empty grave, marked grave dig | shovel; teleport spot revealed | `go to graveyard # find shovel` |
| 4 | Waterfall & boulder | Vine path; marble dislodges boulder | crossing opened; marbles recovered | `go southwest # waterfall area` |
| 5 | Tavern ground floor | Rose, Sammy, Deborah Q&A (dialog-heavy) | Jasper/Gallagos letter clues, cave route | `go to tavern # Deborah, Sammy, Rose` |
| 6 | Tavern upstairs | Carpet key, chest, picture clue | key, book clue, spare marble | `go upstairs # tavern upstairs room` |
| 7 | Library | IQ test rewards | ruler (troll riddle) | `go to library` |
| 8 | Skull well descent | Flower, enter Whispering Caverns | Heliopsis Splendor flower | `go north # skull well area` |
| 9 | Caverns puzzles | Body search; floodgate + spirit-room dart-gun switch puzzles | block, dart gun | `search body # find block and dart gun` |
| 10 | Mausoleum sliding puzzle | Pry slab, sliding-block image puzzle (skippable) | crypt access | `go to graveyard # return to mausoleum` |
| 11 | Catacombs & color dial | Dark rooms, torn pages, 9-input color dial | candle, metal bar, secret-passage letter | `take candle` |
| 12 | Hang glider & lasers | Kite + bar = glider; laser circuit puzzle; Colin | acorn, goggles | `go to eastern bridge area` |
| 13 | Dock house mail search | Goggles word-search for letters | Jasper + Gallagos letters | `go to dock house` |
| 14 | Tavern letter payoff | Deliver letters; buy chicken | chicken (troll), breastplate | `give jasper letter to deborah` |
| 15 | Squirrel ruin & Sarah | Mirror-movement tile puzzle; meet Sarah | Sarah's ring, Gyre backstory | `go to ruin with squirrel` |
| 16 | Troll riddle cave | Chicken repels troll; explosives, shadow measuring, answer 51 | maze access | `go to castle cave entrance` |
| 17 | Maze & trap room | Map-guided maze; arrow traps, furnace, rope swing (lethal; script saves first) | golden bow, torch | `use map # consult Carmyle map` |
| 18 | Word puzzle & storage | "You may pass" word puzzle; storage shelves | darts (for soldiers) | `climb ladder # enter castle storage room` |
| 19 | Soldiers & finale | Dart three soldiers (lethal; script saves first); rescue Julyn; Gyre ending | blindfold rope, ending | `go toward prison` |

**Testing notes:**
- The script has exactly two `save game` lines, each right before a lethal sequence: the trap room (phase 17: arrow plates, furnace, rope swing) and the soldier fight (phase 19: dart timing, including leading a fast runner). A driver should restore-and-retry from those slots.
- The sliding puzzle (phase 10) has a built-in skip: `press immovable block 13 times` enables a skip button — the script includes this as the "if stuck" branch; prefer it for automation.
- The color dial (phase 11) takes 9 single-word inputs in order: yellow, yellow, blue, yellow, yellow, red, red, red, red — each is its own parser line.
- `move table over pressure plate` after the dial matters on veteran difficulty (avoids retriggering the trap); harmless otherwise.
- Phase 19's five identical `take blindfold` lines plus `take blindfold from ground` are intentional repeats (six blindfolds make the rope).
- The soldier and rope-swing sequences are position/timing-sensitive; expect retries and use generous `wait`s.

---

Betrayal Alliance
Basic 
https://github.com/Slattstudio/BetrayedAllianceBook1/blob/f8892e0f2508c11699c8b6a42cadea4572a5135c/Docs/Betrayed_Alliance_Hint_Book.pdf

# Betrayed Alliance Book 1
# Single playthrough, optionals removed

look room # wizard room
take map # required to leave
look mirror # trigger wizard explanation
take marbles # needed for later puzzles
use map # leave wizard room

talk to bobby # pond area
ask bobby about julyn # learn Julyn may be in eastern cave

go east # Leah area
talk to leah # local information

go to graveyard # find shovel
look empty grave # reveal shovel
take shovel # needed for grave/mausoleum/catacomb
look symbol grave # identify marked grave
dig near symbol grave # reveal teleport location

go southwest # waterfall area
pull vine # reveal hidden path
go west # follow uncovered path
go west # continue toward boulder path

look boulder # blocked crossing
roll marble # dislodge mercury marble and move boulder
take marbles # recover marbles
cross boulder # move across new bridge

go to tavern # Deborah, Sammy, Rose
enter tavern # inside tavern
talk to rose # learn about Deborah
ask rose about deborah # learn Deborah needs Jasper's missing letter
ask rose about jasper # remember Jasper name
talk to sammy # learn castle cave route
ask sammy about cave # cave clue
ask sammy about gallagos # learn missing letter name
ask sammy about troll # cooked meat clue

go upstairs # tavern upstairs room
look under carpet # reveal key
take key # chest key
unlock chest # near stairs
open chest # get book clue
look picture # Princess and the Pea clue
look under mattress # find marble if needed

go to library # get ruler
take iq test # earn useful rewards
take ruler # needed for troll riddle

go north # skull well area
take flower # Heliopsis Splendor
enter skull well # Whispering Caverns

search body # find block and dart gun
take block # needed for mausoleum puzzle
take dart gun # needed for switches and soldiers

go north # flies and bridge area
use dart gun on switch # open floodgate
cross bridge # continue

go to spirit room # water/ghost puzzle
use dart gun on bottom switch # create first water barrier
walk west # move into west area
use dart gun on west switch # create second barrier
go through exit # leave safely

go to graveyard # return to mausoleum
go north # mausoleum entrance
use shovel on coffin slab # pry off slab
use block on puzzle board # start sliding puzzle
solve sliding puzzle # restore image
# if stuck:
press immovable block 13 times # enable skip button
press skip button # skip sliding puzzle
enter crypt # descend into underground area

look # locate candle in darkness
take candle # light it with STUPID kit
look table # find note
read note # floor colors are a red herring
cross tile bridge # ignore color-changing tiles
go deeper # second catacomb room

look table # first torn page
take page # first note piece
search sarcophagus # second torn page
take page # second note piece
open cabinet # break off metal bar
take bar # needed for kite/hang glider
look cabinet # inspect fallen cabinet
look hole # hidden letter and picture
take letter # secret castle passage clue
take paper # third torn page
read pages # determine dial sequence

use dial # open crypt mechanism
yellow # dial input 1
yellow # dial input 2
blue # dial input 3
yellow # dial input 4
yellow # dial input 5
red # dial input 6
red # dial input 7
red # dial input 8
red # dial input 9

move table over pressure plate # avoid retriggering trap on veteran
climb ladder # exit catacomb

go to eastern bridge area # far east, then south
take kite # tied to fence

go to windy mountain tree # windy acorn area
take acorn # needed for squirrel puzzle
use stupid kit # combine items
combine kite with bar # make hang glider
use hang glider # fly over scientist laser

solve laser circuit puzzle # connect yellow outputs to blue inputs
enter scientist house # after lasers shut off
talk to colin # scientist
ask colin about goggles # get word-finding goggles
take goggles # essential for dock/mail search

go to dock house # mail room area
enter dock house # search mail room
put on goggles # activate word search
search jasper # find Deborah's letter
take jasper letter # required for chicken
search gallagos # find Sammy's letter
take gallagos letter # needed for Sammy reward

go to tavern # resolve letters
give jasper letter to deborah # unlock Deborah
buy chicken # needed for troll
give gallagos letter to sammy # get breastplate
take breastplate # armor for later

go to ruin with squirrel # nine-tile ruin puzzle
give acorn to squirrel # activate tiles
say copy cat # make squirrel mirror you
follow arrow pattern # use architecture arrows
move through tile pattern # squirrel mirrors movement
enter opened tunnel # route to Sarah

look engraving # G + S clue
dig in tunnel # find ring hinted by Sarah
take ring # Sarah's ring
go to woman in white # Sarah
talk to sarah # learn Gyre context
show ring to sarah # reveal note
take note # learn why Gyre sabotages you

go to castle cave entrance # route Sammy mentioned
enter cave # troll encounter
show chicken # troll hates cooked meat smell
talk to troll # starts riddle
show ruler # needed for measurement answer

search bones # find explosives
take explosives # needed for sunlight
use explosives on ceiling rocks # prepare to blast opening
use flint on explosives # light explosives
wait # sunlight enters cave

use ruler on short pillar # measure 15 mustaches
use ruler on short shadow # measure 10 mustaches
use ruler on long shadow # measure 34 mustaches
talk to troll # answer riddle
say 51 # tall pillar answer
go past troll # enter maze

use map # consult Carmyle map
follow map through maze # main castle route

go through maze # reach trap room
save game # lethal trap sequence
look room # inspect arrows, furnace, rope, torch
take golden bow # heavy item for trap plate
use golden bow on trap plate # trigger/disarm first arrow trap
take arrow # collect shot arrow
use golden bow on second trap plate # disarm second arrow trap
move statue # block furnace face
take torch # now safe to carry light
shoot arrow at rope # cut rope from distance
swing on rope # cross pit

look wall note # clue for word puzzle
read note # phrase clue
put on goggles # easiest solve
search you may pass # solve word puzzle
go through passage # continue toward tower

climb ladder # enter castle storage room
look shelves # find darts
search shelves # collect darts
take darts # needed for soldiers

go toward prison # soldier sequence
save game # action sequence
shoot first soldier # dart him before he reaches you
shoot ahead of second soldier # lead the fast runner
attack third soldier # armored soldier cannot effectively fight
continue to prison # final room

look prisoners # identify situation
look oubliette # Julyn is below
open oubliette # reveal Julyn
take blindfold # prisoner blindfold
take blindfold # prisoner blindfold
take blindfold # prisoner blindfold
take blindfold # prisoner blindfold
take blindfold # prisoner blindfold
take blindfold from ground # sixth blindfold
use blindfolds # make rope
lower blindfold rope # reach Julyn
pull julyn up # rescue Julyn

wait for gyre # final confrontation
drop sarah's ring # distract Gyre
wait # Julyn knocks Gyre into oubliette
watch ending # complete game

