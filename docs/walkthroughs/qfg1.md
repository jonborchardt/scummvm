QFG1
Basic 

ask sheriff about brigands # get town context
ask sheriff about magic # get town context
ask sheriff about otto # get town context
ask sheriff about merchant # learn about Abdulla
ask sheriff about heroes # get town context
ask sheriff about monsters # get monster context

go north # to Hero's Tale Inn
enter inn # meet Shameen
ask shameen about merchant # learn Abdulla arrives later
ask shameen about robbery # learn about stolen goods
ask shameen about food # inn services
sit # trigger Shema
order food # buy meal
eat food # restore hunger
stand # leave table
leave inn # back to street

go west # to magic shop street
enter magic shop # visit Zara
ask zara about erasmus # learn wizard
ask zara about erana # learn Erana
ask zara about baba yaga # learn witch
ask zara about curse # learn curse
ask zara about magic # learn magic
ask zara about spells # learn spells
leave magic shop # back outside

enter guild hall # join adventurer flow
read logbook # get points/context
sign logbook # register hero
look at board # see quests
read quests # learn main goals
ask wolfgang about barnard # missing baronet
ask wolfgang about elsa # missing daughter
ask wolfgang about yorick # brigand warlock context
ask wolfgang about monsters # monster context
ask wolfgang about brigands # brigand context
ask wolfgang about baba yaga # witch context
ask wolfgang about curse # curse context
leave guild hall # back outside

go north # to market street
enter dry goods store # visit Kaspar
ask kaspar about equipment # shop info
ask kaspar about armor # armor info
leave store # back outside

go west # to tavern street
enter alley # find beggar
give beggar silver # get goodwill
ask beggar about night # thief/alley clue
ask beggar about brigands # clue

enter tavern # get note
get note # under stool
leave tavern # back outside

go east # to market
ask hilde about brigands # local info
ask hilde about fruit # needed later
ask hilde about vegetables # food info
ask hilde about father # farm info

wait until sunset # Abdulla arrives
enter inn # meet Abdulla
sit # at table
ask abdulla about brigands # stolen carpet info
ask abdulla about brigand leader # Elsa clue
ask abdulla about minotaur # fortress clue
ask abdulla about name # context
ask abdulla about shapeir # context
give abdulla ration # learn more
pay shameen for room # sleep safely
sleep # restore

leave town # start field work
go east # road
go east # road
go east # near fox area
free fox # Baba Yaga curse clue

go west # toward town
go west # toward town
go west # town entrance
go north # healer hut
knock door # enter healer
enter healer hut # talk healer
ask healer about potions # potion info
ask healer about components # dispel potion info
ask healer about ring # lost ring quest
leave hut # outside tree
get rocks # ammo
throw rocks at nest # knock down ring
repeat until ring falls # continue throwing
get ring # recover ring
enter healer hut # return ring
give ring to healer # get reward
leave hut # outside

go west # farm
ask heinrich about name # farm context
ask heinrich about hilde # market context
ask heinrich about farm # local context
ask heinrich about brigands # brigand context
ask heinrich about leader # brigand leader clue

go east # back to healer area
go north # castle gate
ask karl about castle # castle info
ask karl about baron # Baron info
ask karl about barnard # missing son
ask karl about elsa # missing daughter
ask karl about baba yaga # curse info
ask karl about curse # curse info
open gate # enter castle
go north # courtyard

ask weapon master about fighting # if present
train # optional stat boost

go east # stables
enter stables # work
yes # clean stables

repeat until strong enough:
  fight goblins # build combat and money
  search bodies # collect coins
  rest # restore stamina
  buy healing potions # stay alive
  sleep at erana's peace # full recovery

buy armor # when you have enough money

go to erana's peace # safe meadow
pick flowers # dispel ingredient

go to dryad tree # southwest forest
say yes # accept Dryad quest
go east # toward mushroom ring
go north # mushroom ring
get mushrooms # healer component

go east # toward spirea
go north # forest
go north # forest
go north # forest
go north # forest
go east # forest
go north # spore plants
get rocks # ammo
throw rock at seed # knock seed loose
get seed # Dryad quest item

return to dryad # back to tree
say yes # give seed
get acorn # dispel ingredient

go to meeps # northwest
ask meeps about meeps # talk
ask meeps about fur # clue
ask meeps about green fur # ingredient
get green fur # dispel ingredient

go to waterfall # Flying Falls
get water # flying water in flask
throw rock at door # wake hermit
throw rock at door # continue
throw rock at door # open access
climb ladder # reach door
knock door # enter
enter cave # hermit cave
ask hermit about magic # spell/context
ask hermit about brigands # clue
leave cave # back outside

go to healer # make potion
give acorn # ingredient
give green fur # ingredient
give flowers # ingredient
give flying water # ingredient
give mushrooms # sell/use component

wait until night # fairies appear
go to mushroom ring # fairy location
dance # please fairies
ask fairies about fairy dust # ingredient
get fairy dust # final ingredient

go to healer # finish potion
give fairy dust # ingredient
leave healer hut # allow potion prep
enter healer hut # return
get dispel potion # needed for Elsa

buy undead unguent # needed for graveyard

go to market # buy fruit
buy 50 apples # for Brauggi

leave town # to snowy pass
go east # road
go east # road
go north # pass
go east # Brauggi
say bargain # negotiate
give apples # receive gem
get glowing gem # skull payment

go to baba yaga gate # northwest hut
ask skull about eyes # deal clue
ask skull about deal # payment
say yes # accept deal
ask skull about hut # hut clue
ask skull about rhyme # get rhyme
give glowing gem # open gate
say hut of brown now sit down # lower hut
enter hut # meet Baba Yaga
tell baba yaga your name # dialogue
say yes # accept mandrake task

wait until midnight # mandrake timing
use undead unguent # survive graveyard
go to graveyard # west of town
get mandrake # pull root
return to baba yaga # deliver root
say yes # give mandrake

go to erana's peace # path to ogre
go south # toward cave
go east # ogre cave
fight ogre # clear entrance
open chest # loot
get gold # money
enter cave # bear cave
feed bear # calm bear
go north # kobold room
fight kobold # get key
get key # free bear
open invisible chest # optional loot
get gold # money
return to bear # back room
free bear # Barnard

go to castle # report rescue
enter castle # meet Baron
ask baron about brigands # plot
ask baron about brigand leader # plot
ask baron about warlock # plot
ask baron about prophecy # plot
ask baron about elsa # plot

wait until noon # Bruno meeting
go to archery range # south of town
watch bruno and brutus # learn password
remember hiden goseke # fortress password
enter archery range from south # ambush Brutus
fight brutus # get key
get key # fortress access

go to antwerp area # near fortress path
go around antwerp # avoid hazard
unlock door # use Brutus key
open door # enter tunnel
say hiden goseke # fool troll
enter cave # fortress route

go left # troll treasure
fight troll # clear Fred
search pile # loot
go right # back
go lower right # fortress gate

fight toro # defeat minotaur
search toro # loot
force gate # enter fortress
enter fortress # courtyard

go left # courtyard route
cross right bridge # trap route
step over rope # avoid trap
enter back right door # mess hall

save game # timing puzzle
close door # delay brigands
go to upper right door # trigger movement
move chair # block brigands
move candelabra # block route
climb table # escape path
enter back left door # warlock area

ask warlock about yorick # continue
go through first left door # maze
go through first right door # maze
stop # if rolling
pull chain # open door
go back # return
enter back right door # open route
open painted door # trap door
move aside # avoid falling door
enter painted door # leader room

throw dispel potion # reveal Elsa
get healing potions # desk loot
get mirror # needed for Baba Yaga
leave right # exit fortress

go to baba yaga # final curse removal
say hut of brown now sit down # enter hut
enter hut # confront Baba Yaga
hold mirror # reflect spell

go to castle # ending

https://gamefaqs.gamespot.com/pc/564775-quest-for-glory-i-so-you-want-to-be-a-hero/map/13461-complete-map


https://gamefaqs.gamespot.com/pc/564775-quest-for-glory-i-so-you-want-to-be-a-hero/faqs/12728
Table of Contents

1. Introduction
2. The Story So Far
3. Getting Started
   3A. The Classes
   3B. Character Statistics
   3C. Magic Spells
   3D. Surviving in this Crazy World
4. Spielburg Valley
   4A. Map and Points of Interest
   4B. Characters
   4C. Monsters
   4D. Items
5. Fighter Walkthrough
6. Magic User Walkthrough
7. Thief Walkthrough
8. The Whole Story and Epilogue
9. Miscellany
   9A. FAQ
   9B. Point List
   9C. Frequently Missed Points
   9D. Ways to Die
   9E. Conversation Topics
   9F. Debug Mode
10. Standard Guide Stuff
   10A. Legal
   10B. E-mail Guidelines
   10C. Credits
   10D. Version Updates
   10E. The Final Word

******************************************************************************
1. INTRODUCTION
******************************************************************************

Okay, I'll admit it.  I love Quest for Glory.  I'm absolutely crazy 
about the series.  The greatest thing about the series is the well-done 
connection between RPG and the classic Sierra graphic adventure.  Unlike 
most other of Sierra's adventures, the Quest for Glory series has more 
non-linear elements.  The combat, the spells, the loot, and the silly 
puns and jokes all make for a great adventure.

In this, the first out of five in the series, our nameless hero (well, 
you name him) enters the Valley of Spielburg to prove himself.  This is 
by far the simplest of the five games, but there are still some 
head-scratchers for puzzles.

Note: This FAQ is for the EGA version.  If you have any questions about 
the VGA version, look on GameFAQs.  My VGA FAQ is under a separate heading.

******************************************************************************
2. THE STORY SO FAR
******************************************************************************

Since this is the first game in the series, there's not much backstory.  Our 
hero, who's at this point just a mere adventurer, leaves his home town of 
Willowsby to prove himself as a bonafide hero.  He notices, in his home 
Adventurer's Guild, a notice for a Hero wanted in Spielburg Valley.  What 
looked particularly good to him was the "No Experience Necessary" tag added.  
With that, he set off for Spielburg, and certain adventure.

******************************************************************************
3. Getting Started
******************************************************************************

=================
3A. The Classes =
=================

Once you start a new game, you'll have the choice of picking one of the 
game's three classes.  Any class can easily complete the game, of course, but 
some have harder times at survival than others.

---

The Fighter

Swinger of Swords, Basher of Barriers, Head of Thickness, the Fighter is by 
far the simplest and most straightforward character.  He solves his problems 
with the edge of his blade and can handle almost any monster that crosses his 
path, with a little practice.

Starting Stats:

Strength: 25
Intelligence: 10
Agility: 15
Vitality: 15
Luck: 10
Weapon Use: 20
Parry: 15
Dodge: 10
Throwing: 10

---

The Magic User

While not the strongest of the characters, there is much that is capable 
by the Magic User.  He can weave spells that can do things the other classes 
couldn't even hope to do with only the power of his mind.  Choose the Magic 
User if you like to dabble in the arcane.

Starting Stats:

Strength: 10
Intelligence: 25
Agility: 15
Vitality: 15
Luck: 10
Weapon Use: 10
Dodge: 15
Magic: 25

---

The Thief

My favorite class, the Thief always has to be the most ingenious of the 
classes.  While the Fighter can just muscle his way through things, and the 
Magic User can utilize any of the spells at his command, the Thief is required 
to improvise.  And, of course, there's always the opportunity to add to one's 
wallet.

As a Thief, you should know about the basic tools of survival.  The best help 
you can get is from your fellow Thieves.  Of course, no one goes about asking 
where Thieves are, so a simple method of contact was developed: The Thief 
Sign.  Simply click your Lockpick on a person whenever you suspect they're a 
Thief and they'll respond in kind, and may offer you some aid, or at least 
directions to the nearest Thieves' Guild.

Starting Stats:

Strength: 10
Intelligence: 15
Agility: 25
Vitality: 10
Luck: 10
Weapon Use: 10
Dodge: 5
Stealth: 10
Lockpicking: 10
Throwing: 5
Climbing: 5

---

Once you select your class, you'll have the opportunity to name him, and 
adjust his base statistics.  You have a pool of 50 points, which you can 
distribute to your skills in five-point increments.

If you want to add five points to a skill that your class doesn't normally 
have, you can do so, but you'll have to spend fifteen points for the first 
five.  In this manner, you can give yourself three skills that you could not 
normally have.

Speaking of creating hybrids, I personally don't like it, for every class can 
reach 500 Puzzle Points, and if you want to do particular acts, just use that 
class.  Of course, if you like being able to cast spells while sneaking 
around the Thieves Guild, go right ahead...

==========================
3B. Character Statistics =
==========================

Any time you press click on the swirly bar at the top of the screen, and then 
on your character's portrait in the sub-menu, you'll bring up your Character 
Screen, where you can observe how your stats change.  Here are all the stats 
and how to increase them.  Every stat maxes out at 100.

---

Attributes (Basic Stuff) -

Strength: Your physical prowess, Strength determines how hard you can hit, 
 how well you can muscle your way through things, and how much you can carry.  
 Strength is the most useful for the Fighter, since he spends most of his time 
 using his muscles.  Strength also determines how many Health Points you have.  

- You can increase Strength through physical exercise and through combat 
 (particularly by striking).

Intelligence: Your mental prowess.  Intelligence is most useful for Wizards.  
 Not only does it determine how effective spells are, but also factors 
 directly to Mana Points.

- You can increase Intelligence by casting spells, learning things, 
 solving puzzles, or making good moves in combat (i.e. successfully 
 dodging an attack and counterstriking)

Agility: This is a measure of how well you can move.  A good Agility means you 
 can duck, dodge, and hide better.  Thieves need top-notch Agility to perform 
 their myriad skills.  Agility also factors towards Stamina Points.

- You can increase Agility by performing some of the Thief skills (Climbing, 
 Lockpicking, Stealth), or through combat (particularly Dodging).

Vitality: Although most useful for Fighters, Vitality is good for everyone to 
 have.  This measures your ability to take damage well.  Vitality factors 
 directly to your Health Points and Stamina Points as well.

- You can increase Vitality through most any physical action, particularly by 
 taking damage in combat.

Luck: A very mysterious skill, Luck factors into a lot of things, and is most 
 useful to the Thief.  I'd imagine Luck is drawn against when performing a 
 skill that has a chance of failing, as well as random stuff like monster 
 encounters.

- Increasing Luck is not very well understood.  In this game, Luck seems to go 
 up with most any action you perform, so just keep fighting or performing 
 skills and Luck should increase enough.

---

Skills (Specific Stuff) -

Weapon Use: Everyone has this skill, since everyone has a weapon.  This is the 
 skill of sticking your pointy object into the other guy.  A high Weapon Use 
 factors towards damage caused and successful strikes.

- Increase Weapon Use by attacking in combat.

Parry: Only Fighters have a true need for this skill, since stopping enemy 
 attacks is much easier with a sword or shield than it is with a dagger.  A 
 higher Parry skill indicates a better chance at taking less damage while 
 parrying a weapon.

- Increase Parry by blocking or parrying in combat (more so if you're 
 successful).

Dodge: Everyone has Dodge.  One can't take hits forever, and a good Dodge 
 skill ensures that you'll be able to avoid blows more successfully.

- Increase Dodge by dodging in combat (more so if you're successful).

Stealth: An essential Thief Skill.  No people want their houses robbed by 
 someone who bangs around like a drunk Antwerp.  Having Stealth activates the 
 "Sneak" ability in the "Special" menu, which reduces the amount of noise 
 you make.  In addition to being useful when robbing houses, it's also useful 
 for the wilderness, as you can avoid potential battles while sneaking.

- Increase Stealth by having Sneak active.  Just Sneak everywhere and 
 you'll max it out eventually...

Lockpicking (aka Pick Locks, I hate grammatical errors): To get into the 
 realms of fabulous cash and prizes, you'll need to open door number 1, 
 and your Thief's lock pick helps, but you need the skill to use it.

- Increase Lockpicking by picking on doors.  The best place to do this 
 is the Healer's house, because her door is barred (so you can't actually 
 enter), you won't get in trouble for it, and you can do it in the daytime.  
 You can only get so high picking that door, though, so to top it off, you'll 
 have to pick your nose.  Seriously!  It works!

Throwing: Useful for both Fighters and Thieves, a good Throwing skill can peg 
 a monster with a few extra rocks or daggers before they get close enough to 
 fight up close.  Throwing can also be used to hit or knock off things that 
 are far off.

- Increasing Throwing can come from throwing rocks or daggers.  Rocks, 
 naturally, are much more economic, considering the fact that you can pick up 
 a whole bunch then throw them all repeatedly.  My favorite place for this is 
 the Healer's Hut, as the "grabbing rocks" animation is very quick.

Climbing: Another Thief Skill, you'll need it for scaling walls, trees, 
 gates, ropes, and other non-ladder surfaces.

- Increase Climbing by, naturally, climbing.  Early on, you can practice 
 on the tree outside the Healer's house, but a better place is the town 
 gate.  Once you've plateaued there, you can max it out on the wall to the 
 Brigand Fortress (but only on your first trip, mind you).

Magic: The Magic User's skill at manipulating reality.  Not much else to say 
 about this, generally.  More will be said in the Spells section about each 
 individual spell's effectiveness.

- Increase Magic by casting, casting, and casting some more...

General Health Points -

Health Points: If you hit 0, you die.  Pretty simple.  The maximum amount of 
 Health you have is determined by adding two-thirds of your Vitality to 
 one-third of your Strength.  So, if you have a Strength of 30 and a Vitality 
 of 60, your Health Point max is 50.

Stamina Points: This is your measure of stamina for performing strenuous 
 acts, such as fighting, casting spells, running, throwing, climbing, 
 etc.  Your max Stamina is determined by adding one-half of your Agility 
 to one-half of your Vitality.

Mana Points: Mana is your measure of magical energy.  Each spell uses a 
 certain amount of mana.  Your max Mana is determined by adding two-
 thirds of your Magic Skill to one-third of your Intelligence.  
 Naturally, if your Magic Skill is zero, your Mana Points will also be 
 zero, no matter how high your Intelligence.

Scores -

Puzzle Points: Much like most other Sierra games, you get points for doing 
 special acts.  The max is 500, and every class has its own special way of 
 reaching that high.  In the Miscellany section, there's a Point List.

Experience: This is much like a basic score.  Talk to someone, buy something, 
 kill something, and you'll get some experience.  It's not really worth 
 anything, but once you hit 1000, you'll get tougher monsters during the 
 daytime.

==================
3C. Magic Spells =
==================

While so many games give you a billion offensive spells to eviscerate your 
enemies, Quest for Glory take a different approach.  Sure, the magic users of 
other games can lay waste to any bad guy, but what happens if they misplace 
their keys, or if they need to reach the book at the top of the shelf?  This 
is why Quest for Glory makes the Magic User skilled in a bunch of all-around 
magic.

Each spell can be built up to 100 in skill by casting it repeatedly.

Leyden's Latent 'Lectrical Discharge, aka "Zap"
MP: 3
Found: You start the game with it.
Description: Casting Zap will charge your weapon with magical energy.  
 The next strike will release the energy.  It can only be cast once at a 
 time.  Increasing your skill will increase the damage done with the 
 charged weapon.

"Open"
MP: 2
Found: Zara's shop for 30 silvers.
Description: Open is useful for unlocking simple locks.  With practice, 
 doors can be opened.  Open can also be used for magical items that have 
 been set to react to an Open spell.

Lowenhard's Lariat of Legerdemain, aka "Fetch"
MP: 5
Found: Zara's shop for 40 silvers.
Description: Fetch is useful for grabbing objects that are distant.  It 
 can mostly be used on small, non-living objects.  With practice, you can use 
 it to move objects from one place to another.

"Flame Dart"
MP: 5
Found: Zara's shop for 60 silvers.
Description: Your only attack spell in the game, the Flame Dart produces 
 a small ball of magical flame which you can direct at an opponent, either in 
 combat or while still at a distance.  It can also be used, get this, to burn 
 things.  As you increase in skill, your Flame Darts will do more damage.

"Detect Magic"
MP: 2
Found: Meep's Peep.  Ask the Green Meep about magic.
Description: This is a very general spell, and is used to detect any 
 existing magical auras in the immediate area.

R. Rogers' Reactivating Ritual, aka "Trigger"
MP: 3 
Found: Henry the Hermit's cave.  Ask about magic and the related topics.
Description: This handy little spell can be used to set off any magical 
 spells in the immediate area.  It can make invisble things visible, activate 
 teleport spells, or set off magical traps while the caster is still at a safe 
 distance.

"Calm"
MP: 4
Found: Erana's Peace, under the rock.
Description: Since most Magic Users like to avoid direct combat, you can use 
 this spell to immediately cause any threatening monster to cease its hostile 
 intent and contemplate the universe and its bellybutton.  This works on most 
 enemies with a pulse.  Don't try it on the Undead, and do NOT use it once in 
 actual combat, because a calmed monster is far more dangerous than one 
 blinded by rage.  Having more skill in this spell will cause monsters to be 
 delayed for longer.

Erasmus' Razzle Dazzle, aka "Dazzle"
MP: 3
Found: Erasmus' House.  Beat him in Mage's Maze.
Description: Dazzle creates a brilliant flash of light that will stun 
 anything for a small time while it pauses to rub its eyes.  This only 
 works, naturally, if the creature HAS eyes.  Having more skill in this spell 
 will cause monsters to be delayed for longer.  Although it doesn't last as 
 long as Calm, this skill CAN be used in combat.

===================================
3D. Surviving in this Crazy World =
===================================

Surviving the Keyboard

Yes, this is back in the days of text parsers.  You'll live.  It's helpful to 
know some of the hot key functions, though.  Items in quotes will be entered 
into the text parser as a short cut.

CTRL+A = "Ask about" (follow with a topic to start a conversation)
CTRL+C = "Cast" (follow with a spell name)
CTRL+E = "Escape" (will get you out of combat)
CTRL+F = "Fight" (generally enough to initiate combat with nearby monsters)
CTRL+L = "Look at" (follow with something you want a better description of)
CTRL+I or Tab = Inventory
CTRL+P = Pause Game
CTRL+Q = Quit Screen
CTRL+S = Character Screen
CTRL+T = Time of Day
F3 = Repeat Last Command
F5 = Save Screen
F7 = Restore Screen
F9 = Restart Screen

You can also use the mouse in the game.  Clicking the left mouse button will 
move your character, and clicking right mouse button will look at an object.

Quest for Glory understands most simple verbs.  Just type the verb and its 
object and you should be able to get around fine.

You'll need to use the text parser to converse with people as well, with the 
"ask about" command.  Common things to ask about are: name, hero, magic, 
town, brigands, curse.  Of course, plenty of characters specialize in their 
own field which you'll have to ask specific questions about.  For each 
character (later on, in Miscellany), I'll assemble possible things to ask 
about.

---

Basic Survival

Let's start with SAVE OFTEN!!!

You are a human, and as such, you'll need to eat, sleep, and keep from 
getting hurt too much.  

If you have food rations, you'll eat automatically, and you can eat fruits 
and vegetables from the Market to sate your appetite.  You can also get full 
meals at the Inn, for a price, and there's a tree at Erana's Peace that bears 
fruit that will keep you going the whole day.

Sleeping is another concern.  If you do not sleep for a couple of days, your 
stamina will drop much quicker.  It's generally advisable to sleep every day, 
and to get a full night's rest, you need to sleep before midnight.  Of course, 
you can't sleep just anywhere.

Learn the time scale of the game.  Although you have no clock or anything, 
you'll go by the sun.  There are eight "times" in the game.

Day is Dawning: Town opens for the day.  Day monsters appear.  Ghosts 
  disappear from Graveyard.
Mid-morning
Midday
Mid-afternoon
Sunset Approaches: You can sleep.  Sheriff goes home and Abdulla shows up at 
  the Inn.
Night is still young: Shops close up.  Gate to town closes.  Ghosts frolic in 
  the graveyard.  Night monsters show up.
Middle of the Night: Can no longer get a full night's sleep.  Inn closes for 
  the night.
Not yet Dawn

The best place to sleep is Erana's Peace.  Even if you don't get a full 
night's rest, you'll wake up completely restored.

Other decent places to sleep are the Inn, the Castle Stables, the Dryad's 
Tree, and Henry the Hermit's cave.  Except for the Dryad's Tree, you'll have 
to pay in some way for these places.  It's five silvers a night for a room at 
the Inn, one of your rations (and a few games of cribbage) in the Hermit's 
cave, and a rude awakening (and morning's work) for the Stable.  None of these 
places restore you fully.

If you're absolutely desperate for sleep, you can curl up on the streets of 
the town.  Just don't expect your wallet to be there when you awaken.

Any other place you sleep in is unsafe and will result in your death.

You can also "rest" whenever you want.  But, you'll be too impatient to rest 
after a bit.  Resting will restore several stamina and a few health and mana.

Staying alive in a combat sense is pivotal, of course.  Potions to restore any 
of your three Health Meters can be purchased either at Zara's Magic Shop or at 
the Healer's Hut.  You'll want to buy from the Healer's Hut, because it's 
cheaper.

---

Surviving Combat

Friendly piece of advice: Don't drop (or throw) your weapon.  You're not 
nearly skilled enough to fight unarmed, and you'll be promptly pummeled.

Generally, you'll be set upon by a monster in one of the forested areas in 
the valley.  It'll be running towards you and you can either "fight" it, or 
let it get close and attack you.  If you want, you can throw rocks or daggers, 
or cast spells such as Zap, Flame Dart, Calm, or Dazzle.  You can also just 
"run".

There are two types of combat: Close combat and Side view.

Close Combat is the way you'll fight most often.  You'll be in a black screen 
with the monster in front of you.  You'll need to battle using the keyboard 
arrows.

Up - Thrust
Left/Right - Dodge in that direction
Down - Block or Parry

It's very simple.  Thrust until the monster's dead.  Dodge or Block when 
necessary.  If you wish, you can cast Zap, Flame Dart, or Dazzle.  If you 
feel you're taking too much damage, you can "escape".  Once you escape, you'll 
automatically run for the nearest exit.  Some monsters will give up fast, 
others will chase you until you find a relatively safe spot.

Side view combat is rare.  In this mode, you'll fight right on the normal 
screen.

Up - Thrust or Swing
Left - Dodge
Right - Duck
Down - Parry

After combat, you should always "search the body".  Several monsters carry 
money.

---

Surviving Financially

The currency of Spielburg is the gold and silver coin, with 10 silvers in 
exchange for one gold.  You start your adventure with 4 gold and 10 silver.  
Opportunities for more cash will arise in your adventure:

Renewable Resources:

Working in the Castle: Every day, the manure in the stable piles up.  They 
need someone to clean up.  That's where you, the poor sap, come in.  Every 
time you clean the stables, you pick up five silvers.

Killing Monsters: Most humanoid-type monsters carry money.  Search them after 
you kill them.

Components for the Healer: Finding components for the Healer will earn you 
money as well:

Empty Flasks: 2 silvers
Flowers from Erana's Peace: 5 silvers
Magic Mushrooms: 1 gold
Cheetaur Claws: 5 silvers each
Troll Beard: Two Healing Potions (which is as good as 80 silvers, since 
   you probably were gonna spend it on healing anyway)

Non-renewable Resources:

Finding Treasure: Although not occuring very often, certain caves contain 
treasure that could be very useful to the adventurer on the go.

Thieving: Not only do houses in the area have money, but they also have 
goods that can be fenced at the local Thieves Guild.

Completing Quests: You only get a couple of Quest Completion rewards in-game, 
but they're actually pretty good rewards.

******************************************************************************
4. SPIELBURG VALLEY - *CONTAINS SOME SPOILERS*
******************************************************************************
================================
4A. Map and Points of Interest =
================================

While not terribly big, Spielburg has a lovely charm to it.  Nestled in a ring 
of mountains, the land is home to many a creature, both fair and foul, and 
there are quite a few special locations.  Since this is based on ye olde 
16-color game, every place is divided into screens.  Here's the big main map:

                                     =====                             
                                    ‡  EP ‡                            
                                    ‡     ‡                            
                               ===== ----- =====                       
                              ‡     |     | *C  ‡                    
                              ‡     |     |     ‡                      
                         ===== ----- ===== =====       =====           
                        ‡ SSS |     |     |     ‡     ‡ *EH ‡          
                        ‡     |     |     |     ‡     ‡     ‡          
             ===== ===== ----- ----- ===== ----- ===== -----           
            ‡*BYH ‡     |     |     ‡ *SC ‡     |     |  MZ ‡          
            ‡     ‡     |     |     ‡     ‡     |     |     ‡          
             ----- ----- ----- ----- ----- ----- ===== =====           
            ‡     |     |     |     ‡  CG ‡     |     ‡                
            ‡     |     |     |     ‡     ‡     |     ‡                
       ===== ----- ----- ----- ----- ----- ----- ----- =====           
      ‡     | GCT |     |     |  F  | *HH |     |     |  SF ‡          
      ‡     |     |     |     |     |     |     |     |     ‡          
 ===== ----- ----- ----- ===== ===== ----- ----- ----- =====           
‡  MP |     |     |     |  GY ‡ *ST |  E  |  R  |  R  |  R  ‡          
‡     |     |     |     |     ‡     |     |     |     |     ‡          
 ===== ----- ----- ----- ----- ===== ----- ----- ===== =====           
      ‡     |  MR |     |     |  AR |     |     ‡                      
      ‡     |     |     |     |     |     |     ‡                      
       ===== ----- ----- ----- ----- ----- =====                       
      ‡  DT |  +  |  +  |     |     |   / | *FF ‡                      
      ‡     |     |     |     |     |     |     ‡                      
       ===== ===== ----- ===== ===== ----- =====                       
                  ‡ *AT |     |     ‡  SS ‡                            
                  ‡     |     |     ‡     ‡                            
                   ===== ===== ----- =====                             
                  ‡ *BF |  BA |     ‡                                  
                  ‡     |     |     ‡                                  
                   ===== ===== =====                                  
                                                                       
Key: "-, |" = Screen Edge (passable)
     "=, ‡" = Screen Edge (impassable)
     Letters = Special Location without Random Monsters
     "*" = Location has more than one screen
     "/" = Log.  Only a landmark.  Can still be attacked.
     "+" = White Stag location.  Cannot be attacked.

Points of Interest:

---

"ST" - Town of Spielburg:
 Much like the valley, the town is rather small.  The town was formed first 
 and the castle was built a long time ago by the residents of the town.  The 
 isolation of the valley meant that the population of the town never got very 
 big.  One of the town's major charms is that it's completely safe from the 
 inside.  The Archmage Erana cast a greatly amplified spell of Calm many years 
 ago, which still exists today.  No magic may be cast in the walls, and there 
 can be no aggression towards another, which keeps the town safe from brigand 
 attack.

There are four street screens:

Corner of Main Street and Market Street:

- Sheriff's Office: No one's actually sure if the door even opens.  The 
  Sheriff and his assistant Otto always hang out on the porch.

- Hero's Tale Inn: Run by two Kattas, Shameen and Shema, the Inn is a welcome 
  respite from an adventurer's long day.  Rooms are available for five silvers 
  a night, and food and drink is available at three and one silvers, 
  respectively.

- Barber's Shop: The barber's been out to lunch for some time.

End of Main Street:

- Magic Shop: A enygmatic being named Zara runs the Magic Shop.  She sells 
  items of magic to any who are skilled in the art.  She also sells potions.

  Prices:
  Open Spell: 30 silvers
  Fetch Spell: 40 silvers
  Flame Dart Spell: 60 silvers
  Healing Potion: 50 silvers
  Vigor Potion: 25 silvers
  Mana Potion: 75 silvers

- Little Old Lady's House: A private (and very tempting) residence.

- Adventurer's Guild Hall: A home away from home for adventurers, the Guild is 
  a place to relax and talk about stories.  Since there aren't many 
  adventurers in Spielburg, there's only the Guildmaster, Wolfgang, to talk 
  to.  There is a Quest Board and a logbook here as well.

Corner of Market Street and Tavern Street:

- Farmer's Mart: A small fruit and veggie stand, run by Hilde the Centaur.

  Prices:
  Vegetables: 5 for 1 silver
  Apples: 10 for 1 silver

- Dry Goods Store: Run by Kaspar, this shop contains some staples for 
  adventurers:

  Prices:
  Flask: 2 silvers
  Food Rations: 5 for 5 silvers
  Dagger: 25 silvers
  Chainmail Armor: 500 silvers

- Sheriff's Home: The sheriff's private (and also very tempting) residence.

End of Tavern Street:

- Bakery: The Baker's been gone fishing with the Butcher for some time.

- Butchery: See Bakery

- Alley: Very dark, and colder than the rest of town.  Sam the Beggar hangs 
  out here by day.  More unsavory characters hang out here at night.

- The Aces and Eights Tavern: A place for when one wants some hearty ale.  
  The folks aren't all that nice, but the drinks are terrible.  There is some 
  use for this place, though, if one is of a dishonest nature.

  Prices:
  Ale: 1 silver
  Troll's Sweat: 5 silvers
  Dragon's Breath: 20 silvers

  That, by the way, is all merely a formality.  Dragon's Breath will kill 
  you.  Troll's Sweat will cause you to pass out immediately, and three ales 
  will do the same.  If you pass out, you'll wake up outside with five lost 
  Puzzle Points and an empty wallet.

- Workshop: In the EGA version, this was an odd building no one ever figured 
  out, since it seemed to be occupied, but in the VGA version, it's obvious 
  that it's quite abandoned.

- Thieves' Guild: It's in this area somewhere; a place where the thieves in 
  the area can relax without worrying about being thrown in the slammer.  You 
  can fence your stolen goods, or play a little Dag-Nab-It to get some extra 
  cash if you're skilled.  There's also a modest set up for fencing and a 
  shop for Thief items:

  License: 25 silvers
  Lock Pick: 15 silvers
  Tool Kit: 100 silvers

---

"E" - Entrance to Town

Sometimes an unsavory-looking character will be outside the gate.  This is 
Bruno, the seller of information.  He may be willing to help, for a price.  
Just take his advice with a grain of salt.

Prices:
1 silver - Directions to Goblins, Baba Yaga, or the Castle
1 gold - Thieves' Guild (Lie)
2 gold - More info about Baba Yaga (Useful, but obtainable elsewhere)
10 gold - Brigand Fortress (Misleading)

---

"R" - Road to Mountain Pass

Nothing much going on here except for the fact that you can't be attacked 
here.  You may spot a trapped animal on one of the screens.

---

"HH" - Healer's Hut

The Healer resides here.  She can sell you several different potions, and can 
help you make some of the more complicated ones.  You can also sell 
components to her:

Prices:
Healing Potion: 40 silver
Vigor Potion: 20 silver
Magic Power Potion: 60 silver
Undead Unguent: 100 silver

Outside, you'll see a nest with a small Pterosaur in it.  This is the nest of 
the Healer's pet pterosaurs.

---

"F" - Farm

This is the farm of Heinrich the Centaur.  Not much you can do here but talk 
to him.

---

"CG" - Castle Gates

You can speak to Karl, the gate guard, about may of the things going on, or 
you could just ask him to let you in by clicking the Hand on the gate.

---

"SC" - Spielburg Castle

Built a long time ago, Spielburg Castle is an impressive sight, but is a bit 
small as far as castles go.  There is just a main wall and a keep.  The Barony 
of Spielburg inherited the castle from King Siegfried the Third two centuries 
ago.  Baron Stefan von Spielburg lives here, and, at the moment, he lives 
alone with his sadness (and a few guards).

- Courtyard: Sometimes, one can find the Weapon Master here, practicing.  
  He'll give lessons to aspiring Fighters for one gold a lesson.

- Stables: East of the Courtyard, you'll find the stables.  The Stable 
  Hand is more than willing to let you work in the stables cleaning up 
  the place.  He'll pay you five silvers for each time you clean it out.

- Barracks: West of the Courtyard is the Barracks for the guards.  There's not 
  much to do there except be told by the guard to leave.

- Keep: North of the Courtyard is the door to the Keep, which opens for 
  no one, unless they're important.

- Great Hall: Not very flashy, but if one does well, one may see the 
  place.

---

"AR" - Archery Range

A lonely little archery target sits on the town's south wall.  While you 
have no bow, it is a good place to practice throwing daggers.

---

"GY" - Graveyard

A neglected and run-down place, the Graveyard hasn't been tended to in 
years since Baba Yaga cursed it.  What was the curse?  Take a wild guess.

---

"SF" - Snow Field

This area contains a pass through the mountains, but it's very treacherous 
for all but the most skilled, or giant-like, of spelunkers.

---

"FF" - Flying Falls

The magnificent Flying Falls are a wonder to behold.  On one of the walls of 
the cliffside is a door that must lead to someone's residence, but it's atop 
a ledge that's several feet up.  It's rumored that a Hermit lives there.

---

"SS" - Spiegelsee

Also called Mirror Lake, you can pause here for some peaceful reflection, and 
maybe see a sight gag or two, but that's it.

---

"MZ" - Mount Zauberburg

Magic Mountain is the home to a powerful, yet humorful, Wizard named Erasmus 
and his familiar Fenrus.

---

"EH" - Erasmus' House

After a tough climb up Mount Zauberburg, one must match wits with the 
Gargoyle watching the gate.  Answering three questions correctly will allow 
you to see the Wizard.

---

"C" - Cave

Few people know what's inside the cave in this northeastern corner of the 
valley.  They probably haven't the guts to go in given the huge Ogre standing 
in front of it.

---

"EP" - Erana's Peace

Rumored to be the final resting place of the Archmage Erana, this is a very 
safe meadow.  There are many flowers of different varieties, a tree that bears 
fruit that can satisfy the most ravenous of hungers, and a gift for those 
knowledgeable in magic.

---

"SSS" - Spore Spitting Spirea

On a rocky ledge rest four very rare plants.  These plants spit some odd 
sort of seed around.  Naturally, nothing this weird exists unless you have 
some use for it, right?

---

"BYH" - Baba Yaga's Hut

As you'll see, it's not an easy place to get into.  The skull guarding the 
gate will force you to bargain with him, and even if you can get inside the 
hut, you may find out you didn't want to.

---

"GCT" - Goblin Central Combat Training Zone

If ever you needed an endless supply of Goblins, this would be the place to 
find them.  Every time you fight here, the number of Goblins that attack you 
increase by one.  You can get some good money from them.

---

"MP" - Meep's Peep

Fuzzy Meeps live in the holes here.  They have a generally positive outlook 
on life, but they're rather shy.  Only the rare Green Meeps have any capacity 
for conversation.

---

"MR" - Mushroom Ring

A circle of mushrooms is in this area.  You can give these mushrooms to the 
Healer, or you can visit at night for an interesting light show.

---

"DT" - Dryad Tree

In a small corner of the forest, the Dryad, the protector of the woods, 
sleeps.  She is known to ask favors of those she thinks are friends of 
the forest, and is willing to return those favors.

---

"AT" - Antwerp Area

This has to be seen to be believed.  There may be more to this place, 
however, than that big blue bouncy thing.

---

"BA" - Brigand Ambush

Their fortress must be nearby, because the brigands in the area have set 
up a very nasty ambush in this area, to fill any intruders with arrows.  
A strong hero could push through this ambush.  A smart hero may avoid 
it altogether.

---

"BF" - Brigand Fortress

Well, we assume it's there, since there's an ambush nearby.  Built halfway 
into the mountainside, and protected with fancy fortifications, this 
fortress is ready to withstand the nastiest of assaults.  If you plan to get 
through this place alive, you'll need to use brains and skills.

================
4B. Characters =
================

Some are good, and some are bad.  Some are more than what they seem.

---

The Hero (Ego) - (human male from Willowsby)
 Fresh from basic training as your home Adventurer's Guild, you need a chance 
 to prove that you've got the guts and the talent to change this world.  Much 
 about your past is hidden, and doesn't really need to be revealed, since one 
 needs to identify with his character, and the less known, the better.  You're 
 a strapping, dirty-blonde-haired man in his late teens or early twenties.  
 You wear a white shirt, brown trousers, and a brown belt and shoulder strap 
 for carrying equipment.  For the added flair, you also sport a red cape.

---

Townsfolk:

Schultz von Meistersson, aka The Sheriff - (human male from Spielburg)
 A former adventurer, Schultz decided to settle down after the day to day 
 became too much hassle.  He married a local town girl, and was installed as 
 Sheriff of Spielburg.  He prefers to do the thinking half of the job, leaving 
 the rest to his able-bodied assistant, Otto.

Otto von Goon - (Goon male from Spielburg)
 Goons aren't very bright, but they can be housebroken, much like pets.  
 Otto's not a monster per se, but he's pretty darn strong.  He's the muscle 
 half of the law enforcement team in Spielburg.

Sheriff's Wife - (human female from Spielburg)
 She's mostly implied.  She looks after the house while the Sheriff's at work.

Shameen - (Katta male from Raseir)
 Katta are a humanoid feline species.  Shameen, in the language of the Katta, 
 means "He of the Lion".  He established the Inn in the town of Spielburg 
 hoping to find a Hero to save his land (but that's a whole different game).  
 Now he wishes to head back to his homeland, but he's stranded in Spielburg, 
 due to the attack on his friend Abdulla.

Shema - (Katta female from Raseir)
 Shameen's wife and soulmate, her name means "She of the Dance".  She 
 cleans and cooks at the Inn and serves meals.

Abdulla Doo - (human male from Shapeir)
 A Master Merchant and trader, Abdulla traveled to Spielburg to sell his 
 wares and pick up Shameen and Shema on his way back to Shapeir.  
 Unfortunately, he was attacked a week before the Hero's arrival by the 
 brigands in the valley.  His guards were overwhelmed and he was left 
 penniless.  He's now living at the Inn, a burden on his friends.

Zara Shashina - (half-Faery Folk female from Western Woods)
 A powerful magic user, Zara tends the Magic Shop in Spielburg.  She's 
 very no-nonsense, and is quite blunt and to the point.

Damiano - (Familiar)
 Zara's familiar, Damiano, has the appearance of a man-bat.  It doesn't 
 speak at all.

Little Old Lady - (human female from Spielburg)
 An old widow who lives on her own with her kitty.

Wolfgang Abenteuer, aka The Guildmaster - (human male from Spielburg)
 Wolfgang runs the Adventurer's Guild because, frankly, someone needs to.  
 He's had his share of adventures, and is more than willing to speak about 
 them to younger adventurers.

Hilde Pferdefedern - (Centaur female from Spielburg)
 This young Centaur runs the Farmer's Mart in town.  She's rather shy, but she 
 does a good job peddling the wares.

Kaspar - (human male from Spielburg)
 Kaspar tends the Dry Goods Store in Spielburg.  He's not very active, but he 
 runs a good store.  He has stars in eyes about one day being an adventurer.

Silas Sourdough, aka The Baker - (human male from Spielburg)
Butch Beefmeister, aka The Butcher - (human male from Spielburg)
The Barber - (human male from Spielburg)
 Since these gents have been cut off from supplies for a while with the 
 winter, they have nothing better to do than hang out at the Tavern.

Sam the Beggar - (human male from Spielburg)
 Poor Sam's hit the skids.  He's taken to begging on the streets.  Offer 
 a silver to him, and he may give you some useful information.

Tavern Keeper - (human male from Spielburg)
 He serves drinks.  You want a drink?  Order one.  You want to know about the 
 tavern keeper, you can forget it.

Crusher - (Goon male from Spielburg)
 Crusher maintains his quiet dignity by guarding the entrance to the Thieves' 
 Guild.  He's the Guild's personnel manager.

The Chief - (human male from parts unknown)
 The Chief Thief's not happy with his position, but he'll have to live with 
 chiefing a podunk town until the pass clears.  There's not much for him to do 
 except to play Dag-Nab-It, which you can play with him.

Boris - (human male from Spielburg)
 The Thieves' Guild's accountant, Boris provides thieves with their essential 
 tools for survival in the world of purloining, as well as acting as a fence 
 for valuable items.

Sneak and Slink - (human males from Spielburg)
 These two thieves, seemingly joined at the hip, patrol the alley.  They set 
 a trap for mugging anyone who wanders into the alley.  Being active thieves, 
 they know all the latest passwords for getting into the Guild.

---

Just Outside of Town:

Bruno - (human male from Spielburg)
 Bruno's a member in good standing in the Spielburg Thieves' Guild.  Since 
 there's not much work here, Bruno spends his time outside the main gate of 
 town, selling information to those who'd buy.  He's a pretty dangerous 
 fellow, however, and he's rumored to not be completely loyal to even the 
 Thieves' Guild.

Amelia Appleberry, aka The Healer - (human female from Spielburg)
 She's a pretty darn good healer for such a small area.  She lives in her hut 
 just outside of town and sells her potions.  She has a pet pterosaur, Pterry, 
 who has a mate, Pteresa, who lives in the tree outside her house.  She'll 
 pay for components, and she recently lost a rather valuable ring.

Heinrich Pferdefedern - (Centaur male from Spielburg)
 He tends his farm outside of town.  He makes a living selling the produce he 
 harvests in town.  One of his claims to fame having survived an onslaught by 
 the brigands.

---

Castle Dwellers:

Karl - (human male from Spielburg)
 The gate guard, Karl knows much about what goes on in the castle.  He'll 
 open the gates for anyone who's not a monster.

Weapon Master - (human male from parts unknown)
 A very skilled, but rather arrogant man, the Weapon Master taught all the 
 guards how to fight.  He is extremely gifted with a sword, and will teach 
 anyone else with a sword for a gold a lesson.

Stable Hand - (human male from Spielburg)
 Not much of a conversationalist, but he'll give you work if you want it.

Frederick and Pierre - (human males from Spielburg)
 They guard the castle keep, and that's about it.  Don't mention Pierre's 
 bald spot.

Baron Stefan von Spielburg - (human male from Spielburg)
 The 12th Baron of Spielburg Castle, the Baron has had many hardships.  Ever 
 since the loss of his son and daughter, he has shut himself in his castle 
 and will see no one.

Baronet Barnard von Spielburg - (human male from Spielburg)
 Five years ago, Barnard went off hunting.  Several days later, his horse 
 returned with vicious claw marks.  No sign of the Baronet was ever found.

Elsa von Spielburg - (human female from Spielburg)
 Ten years ago, soon after Baba Yaga cursed the Baron, Elsa was stolen away by 
 some flying creature.  No one knows what became of her.

Yorick - (human male from Spielburg)
 After some time, everyone gave up the search for Elsa, except for the court 
 jester Yorick.  He was a man of infinite jest.  No one knows where he is now.

---

Other Folks in Spielburg:

Brauggi - (Frost Giant male from Jotunheim)
 He's a hungry giant.  He's come far and wide to search for food to mellow his 
 mead horn, and his search has brought him to the valley, specifically, the 
 Snow Field.

Erasmus - (human male from parts unknown)
 From his house atop Mount Zauberberg, Erasmus the Wizard keeps an eye on the 
 happenings in the valley.  While he prefers not to meddle directly in 
 current affairs, he's more than willing to help others trained in the 
 mystical arts, or even just offer passersby some useful information.  If you 
 have the skill, he may even challenge you to a game of Mage's Maze.  Beware 
 his wit, however, for it's sharper than any blade you carry.

Fenrus - (rat familiar)
 Erasmus' familiar is Fenrus, a talking rat.  The two are constantly trading 
 jokes and jabs back and forth, and have developed one of those special 
 relationships that only exist between Wizards and Rat Familiars.

Henry the Hermit - (human (I suppose) male from Spielburg)
 'e lives in 'is cave near Flying Falls.  'e'd be 'appy to share his knowledge 
 of whatever with you, if you can reach 'is 'ome, sez I.  Just be careful you 
 read 'is manner of speaking correctly.

Meeps - (Meep males and females from Spielburg)
 The Meeps are happy Meeps living in their happy holes.  Don't worry, be 
 happy! ^_^

Bonehead - (undead/enchanted)
 Don't know exactly how undead Bonehead is.  Maybe he's just an enchanted 
 talking skull (like Murray).  At any rate, he's posted on Baba Yaga's gate, 
 and he won't let anyone in who doesn't deal with him.

Baba Yaga - (Ogre female from Surria)
 Y'know, you just can't settle your flying chicken hut down anywhere these 
 days without someone trying to get you to leave.  Several years ago, Baba 
 Yaga left her homeland and journeyed to Spielburg.  When the Baron threatened 
 her to leave, she cursed him, and he lost his son, daughter, and many of his 
 guards.  She now lives cozily there, content that she'll be at relative 
 peace.

Dryad
 The Dryad is a tree nymph that watches over the forest.  She is a bit 
 tempermental and to the point, but she does have a soft spot for magical 
 balance in a land (which Spielburg needs in spades).

Brigand Warlock - (human male from parts unknown)
 Not many people ever get a good look at the Warlock, but he's supposed to be 
 a powerful magic user, with the ability to subdue enemies before the main 
 force of the brigands sets upon them.

Brigand Leader - (???)
 No one knows what the Leader looks like, because he goes everywhere with 
 a full helm and cloak on.  He surfaced a few years back and changed the 
 brigands from a petty nuisance to a major force to be reckoned with.  One 
 will need to be particularly skilled to defeat him.

==============
4C. Monsters =
==============

The general definition of a monster is "that which attacks without asking 
questions first".  Every adventure's got 'em, and this one has quite a few.  
The difficulty is rated out of 10, with one being a cake walk, and ten being 
something huge that'll kill you unless you're very skilled.

---

Random Monsters

These monsters can attack you in any wooded area.  They'll attack from either 
the left or the right side of the screen.  Humanoid monsters will chase you 
for a while, but the rest will give up as soon as you go up or down a screen 
(they don't have up or down sprites ^_^).

Goblin - A squat, ugly, blue-green humanoid.  They have little armor, but 
 they wear a horned skull cap, carry a wooden shield, and wield a small mace 
 to attack with.  They attack only with a simple swing.  They're not all that 
 hard to kill, but, in the GCT, they can gang up on you.  They carry anywhere 
 between 1 and 10 silvers. -- Difficulty: 2

Purple Saurus - These descendants of the ancient dinosaurs don't have much 
 brain, but they have a nasty set of teeth, which they'll bite you with, 
 repeatedly.  They only attack in the daytime. -- Difficulty: 3

Brigand - The terror of Spielburg; these guys wear helms, leather armor, 
 carry a round steel shield, and attack with a spear.  Their attack consists 
 of stabbing.  Some brigands have different faces than others, but they're 
 all the same.  You shouldn't take them on too early.  They carry between five 
 and twenty-five silvers. -- Difficulty: 5

Mantray - This is a rather magical creature.  It lives on the land and looks 
 like a cross between a Sting Ray and a Manta Ray.  It hides on the ground 
 waiting for its prey.  Then it flies up and attacks.  It uses its tail as a 
 weapon, which has an electrical current running through it.  It only appears 
 at night until you reach 1000 exp, when it'll appear at the daytime as well.  
 -- Difficulty: 5

Cheetaur - This guy is nasty with a capital NASTY!  Think of it as a centaur, 
 only with the body of a panther instead of a horse, and the upper body is 
 catlike, only shaped like a human.  Like the Mantray, it will only appear at 
 night until you hit 1000 exp.  It attacks hard, and with both hands, so hits 
 are very frequent.  After you beat one, take its claws.  It carries anywhere 
 between 2 and 7 claws. -- Difficulty: 9 

Saurus Rex - The Big Saurus!  Although not as nasty as a Cheetaur, they run 
 faster than you, so outrunning them is tough.  A big set of chompers can 
 make short work of you, and it can absorb hits.  They only appear at night 
 until 1000 exp, much like the Mantray and Cheetaur.  The fact that they 
 don't carry anything makes them not very pleasant targets. -- Difficulty: 8

Troll - Probably one of the more resilient enemies you'll meet, Trolls have 
 rock-hard gray skin and carry huge clubs.  They cause damage, but they don't 
 attack as often as Cheetaurs.  They only come out at night, end of story on 
 that one.  They carry between 25 and 50 silvers, and their beard is very 
 useful for the Healer (worth two Healing Potions). -- Difficulty: 8

Unique Monsters

There are only one of each of these monsters, and they tie into the story for 
the most part.

Ogre - A nasty-looking fella, the Ogre has purple skin and carries a massive 
 hammer, along with a chest filled with his belongings.  He's pretty tough, 
 but not all that bad, particularly if you rest up prior to fighting him.  
 His chest has 1 gold and 43 silver in it. -- Difficulty: 7

Bear - Although not really a monster, this thing will attack you.  I suggest 
 not attacking back.  Well, you could, but save first. -- Difficulty: 7

Kobold - He's a powerful magic user, and he's not really very friendly.  He's 
 also the only bad dude you fight in side view.  It's not necessary to fight 
 him directly unless you are the Fighter, though.  He's not particularly 
 tough in that sense, unless you're rather weakened. -- Difficulty: 6

Minotaur - The mythical cross between a man and a bull, the Minotaur is the 
 toughest monster in the game.  Only the Fighter needs to attack him.  The 
 Minotaur attacks you with a flail, and he absorbs hits quite well.  His 
 flail has 50 silvers in it. -- Difficulty: 10

Other Hazardous Creatures

Antwerp - An odd creature from God Knows Where, this bouncy being inhabits a 
 spot in the south.  Throwing and casting stuff at him doesn't work, and 
 fighting him could produce nasty results.  It's best just to try and leave 
 him alone.  If you DO fight him, he'll bounce very high up and out of sight.  
 He'll drop down on the next screen and squash you unless you click your sword 
 or dagger on him.  He'll explode and you'll cause an Antwerp POPulation 
 explosion.

Ghost - Phantasms composed of ectoplasm and weird stringy stuff.  They can 
 suck the life right out of you if you're not protected with Undead Unguent.  
 Fortunately, they only hang out at the Graveyard at night.

Night Gaunt - Personally, I think these guys are fantasy.  They're supposed to 
 be the "bump in the night" creatures that get you if you bed down in an 
 unsafe place, but that really makes no sense.  Why have a special creature to 
 eat you?  No, I believe they're just stories to frighten young children to 
 sleep, and if you bed down in an unsafe spot, it's just a random monster that 
 munches on you.

===========
4D. Items =
===========

Here's where I collected and compiled all the items that exist in the 
game and what they're used for.  Take care, for looking up some of these 
items can result in plenty bad spoilers.

---

Combat Carryables:

Broadsword -
Found: Start Fighter Quest with one.
Description: Your right hand as a Fighter (assuming you're right-handed), the 
 sword is your main weapon in this crazy battle called life.  It does 
 generally more damage than a dagger.

Shield -
Found: Start Fighter Quest with one.
Description: A big piece of metal that you can stick between you and a set of 
 big nasty teeth, or thrusting pointy sticks.

Dagger -
Found: Start Magic User or Thief Quest with one.  Purchase at Dry Goods for 
 20 silvers.
Description: A small knife.  It's useful for throwing, and can be used in 
 close combat in a pinch.  Thieves should have a good supply of these.

Leather Armor -
Found: Start any Quest with one.
Description: Your basic armor.  It's made from strips of leather boiled and 
 hardened in wax.  It doesn't take much to pierce it, but it's better than 
 going naked (and a lot less disconcerting).

Chainmail Armor -
Found: Dry Goods for 500 silvers.
Description: Made from tiny rings of steel, it's heavier than leather, but not 
 so heavy that it impairs movement too much.  You'll take less damage while 
 wearing this.

---

Survival Staples:

Food Ration -
Found: Start any Quest with five.  Purchase at Dry Goods at five for five 
 silvers.
Description: Not too tasty, but the rations will keep you going throughout the 
 day.

Vegetable -
Found: Farmer's Mart at five for a silver.
Description: Vegetables left over from last year's harvest.  Not enough 
 for a balanced diet, but other things besides you need to eat.

Apple -
Found: Farmer's Mart at ten for a silver.
Description: Apples left over from last year's harvest.  Again, not enough 
 nutrition to keep you alive, but they could be useful in other ways.

Potion of Healing -
Found: Purchase in Healer's Hut for 40 silvers, or Magic Shop for 50.  Also 
 can be found in other places.
Description: A mixture of restorative herbs and country goodness, healing 
 potions will restore about half of your Health Points.  When used, you'll be 
 left with a flask.

Vigor Potion -
Found: Purchase in Healer's Hut for 20 silvers, or Magic Shop for 25.
Description: Caffeine, Vivarin, Jolt, Espresso Beans, Mountain Dew, and GNC 
 Mega Vita Fuel, all in one little flask.  When used, you'll be left with a 
 flask and completely restored Stamina Points.

Potion of Magic Power -
Found: Purchase in Healer's Hut for 60 silvers, or Magic Shop for 75.
Description: Just brimming with electricity, these potions will restore about 
 half your Mana Points.  Like other potions, it will leave a flask behind.

---

Thief Thingies:

Lock Pick -
Found: Begin Thief Quest with it, or purchase at Theives' Guild for 15 
 silvers.
Description: While it seems to be no more than a thin shaft of metal, this is 
 your ticket to realms of unimaginable treasures, with enough practice.

Thieves' Guild License -
Found: Purchased in the Thieves' Guild for 25 silvers.
Description: Rather necessary for the Thieves' Guild not to tip off the fuzz 
 about you.  It also allows you purchase supplies and fence your stolen items 
 at the Guild.  Dental plan!

Thieves' Tool Kit -
Found: Purchased in the Thieves' Guild from 100 silvers.
Description: The short story is that it gives you a higher lockpicking 
 success rate.  The long story is that this kit contains everything from 
 skeleton keys, to thin pieces of wire, to credit cards.

---

Stuff for Stealing:

Alabaster Vase -
Found: In the Sheriff's House
Description: While rather fragile, this large vase can net you some cash.

Golden Candelabra -
Found: In the Sheriff's House
Description: Let's see.  Could this be better used for candles, or as a dust 
 collector in the Thieves' Guild's stash?

Silver Candlesticks -
Found: In Little Old Lady's House
Description: See Golden Candelbra

Music Box -
Found: In the Sheriff's House
Description: One of Otto's favorite toys, but it's rather valuable.  He'll 
 live without it.

String of Pearls -
Found: In Little Old Lady's House
Description: They just don't suit you.  Better off selling them to the Guild.  
 The good news is, they're real.

---

Items for Ingredients:

Flowers -
Found: Erana's Peace
Description: Some magic keeps them perpetually fresh.  Not bad, eh?  Sell 
 them to the Healer for five silvers per bunch.

Magic Mushrooms -
Found: Mushroom Ring
Description: They're colored a rather hazy purple.  It's advisable not to eat 
 them.  You can, however, sell them to the Healer for one gold per handful.

Cheetaur Claw -
Found: Dead Cheetaurs
Description: Killing Cheetaurs is tough, but it's well worth the reward of 
 claws, which can net you 5 silvers apiece from the Healer.

Troll Beard -
Found: Dead Trolls
Description: Trolls take quite a beating, but, in addition to whatever you 
 find on their dead bodies, you can give their beards to the Healer for two 
 Healing Potions.

Kobold Mushroom -
Found: Cave
Description: These are baaad 'shrooms.  They're mostly poisonous.  Don't 
 even bother getting them, they're useless.  Supposedly, if you give them to 
 the Healer, she won't accept the other mushrooms from you.

---

Miscellaneous Megusalems:

Piece of Paper -
Found: Any place you get spells, or the Tavern.
Description: This is a complex interweave of wood fiber, but it's rather 
 useless.

Empty Flask -
Found: Purchase at Dry Goods for 2 silvers, or get whenever you drink a 
 potion.
Description: A carefully blown piece of glass.  It's rather useful for 
 carrying liquids and powders.  If you have extra, you can sell them to the 
 Healer for 1 silver each.  Reduce, reuse, recycle.

Flask of Water -
Found: Spiegelsee
Description: Water's not all that useful, but it exists, so there ya go.

Rock -
Found: Click the hand on most any patch of bare ground in the forest.
Description: Cheap and handy throwing weapons.  Rocks can give a bad guy a 
 nasty bump before you actually hurt him.

---

Puzzle Pieces:

Golden Ring -
Found: Nest outside Healer's Hut
Description: This is the Healer's lost ring.  Give it back for your reward of 
 six gold and two Healing Potions.

Glowing Gem -
Found: Snow Field, given by Brauggi in exchange for 50 apples.
Description: A gem that glows like the flare of the frost flame.  Use it to 
 light up someone's life.

Undead Unguent -
Found: Healer's Hut for 100 silvers
Description: Rub this unguent on your body to repel the undead.  Normally, 
 they repel you.  It doesn't last long, so use it just before you enter a 
 dangerous undead situation.

Mandrake Root -
Found: Graveyard
Description: This root's has rather powerful magical properties.  According to 
 the legends, it must be pulled from a dead man's grave at midnight to be 
 useful.

Spirea Seed -
Found: Spore Spitting Spirea
Description: This simple seed seems to be solid, so don't sell the sucker, 
 since you should send it to some sexy sapling.

Magic Acorn -
Found: Dryad Tree
Description: Well, who knows exactly what it does, but it's an ingredient for 
 the Dispel Potion, in exchange for the Spirea Seed.

Flask of Flying Water -
Found: Flying Falls
Description: Now, it'll only say "Flask of Water", but if you pick it up, 
 it'll be the proper water.  It's an ingredient for the Dispel Potion.

Handful of Green Fur -
Found: Meep's Peep
Description: It's cute, fuzzy, and an ingredient for a Dispel Potion.

Flask of Fairy Dust -
Found: Mushroom Ring (night)
Description: Mysterious stuff.  For all we know, it could be the Fairy's 
 excrement.  Ick.  Good thing it's in a flask.

Dispel Potion -
Found: Healer's Hut
Description: This mystical brew contains the power to dispel any enchantment 
 or shape-change spell.

Large Brass Key -
Found: Cave and Archery Range
Description: There are two in the game, but they're the "same" item.  The 
 first disappears after you use it, and they're used for two different 
 things.

Magic Mirror -
Found: Brigand Fortress
Description: A face (not yours) is on the mirror.  This vanity item is used 
 for reflecting spells back on the user.

******************************************************************************
5. FIGHTER WALKTHROUGH
******************************************************************************

Points added will be shown in parentheses.

As a Fighter, you'll be doing a lot of such, so you'll probably want to boost 
your Strength, Vitality, and Weapon Use.  Of course, if you're going for max, 
you'll probably want to dump it all in Intelligence, since that's the hardest 
stat to raise.

You'll start with a Broadsword, a Shield, Leather Armor, five Rations, four 
gold, and 10 silver.

---

Town of Spielburg:

You'll stroll into town (1), take a short look around, and the Sheriff will 
greet you.  Question him for a bit (1).  First good place to head is to the 
Hero's Tale Inn.  You can question Shameen there (1).  Have a sit down and 
Shema will show up.  You can ask her a few things.  Order some food (1).  Be 
sure to ask one or both of them about Abdulla, to know that he'll show up 
later that night.

Leave the Inn and head west to the end of Main Street.  Enter the Magic Shop 
(y'know, the place with the big eye above it).  Meet and question Zara (1).  
Since you're not a practitioner, all you're really here for is the point you 
get for questioning her.  Leave the Magic Shop.

Head to the Guild at the end of the screen.  Enter and read the Adventurer's 
Logbook (4).  Sign it, too (1).  Take a look at the Quest Board, too (6).  
Wake up the Guildmaster, Wolfgang.  Question him (1) about heroes, monsters, 
and the stuff on the Quest Board.  Question him about the "curse", too, and 
related topics.

Once you've finished with that, leave and go the other way on Main Street.  
You'll eventually reach Market Street.  Speak to Hilde the Centaur (1).  If 
you wish, you can buy 50 apples (3).  At five silvers total, you should 
easily be able to afford it, but it could weigh you down considerably.  Also, 
head into the Dry Goods Store and speak to Kaspar the proprietor (1).  Buy 
some food if you wish.  As you'll notice, you won't have enough to buy armor.  
Deal with it for now.  You'll have enough, eventually.

Head west to Tavern Street.  Head into the alley.  You'll find Sam begging on 
the street.  Give him a silver (1), and you'll be able to question him (1).  
Ask him about night and the brigands.  Also, ask some other topics related to 
what he brings up.

Walk into the Tavern.  Note the odd white thing next to the center stool.  
That's a note.  Pick up the note, and you'll learn some interesting things 
about two guys named "B" (2).  You can order some ale if you want, but don't 
drink three glasses of ale, or any Troll's Sweat (-5), or Dragon's Breath 
(too hot to handle).

That's about all there is to do inside town.  It's probably not Sunset yet, 
so you can go outside for a bit.  When Sunset rolls around, go to the inn, 
where you'll find Abdulla Doo lost in his own sorrows.  Have a seat.  You can 
order some food now if you haven't already.  Have a chat with Abdulla (5).  
Ask him about the brigands and his attack.  Order some food for him (when 
Shema's near) or give him a ration (2).  He'll tell you about his magic carpet 
and Shapeir, giving you some foreshadowing of your next quest.  Might want to 
deal with this one first.  Anyway, you probably should sleep at the Inn 
tonight (1).

The next morning, head out to start your adventure proper.

---

Outside the Town:

You're now outside of town, whistling to yourself (1).  Just as a quick note, 
be sure to always keep an eye on the time so you know when to bed down.

First order of business is to head down the road.  You'll eventually reach the 
end of the road, where a recent avalanche has blocked the pass.  No easy way 
out of this quest.  Head back along the road.  Walk back and forth between 
road screens and eventually you'll come upon a fox trapped near the road.  
He'll ask you to free him.  Do so, and he'll give you a piece of advice and 
info (10).

Head back towards the gate.  There's a chance there'll be a guy outside the 
gate.  This is Bruno, the seller of info.  You can try asking him things, but 
he'll want money first.  Give him a silver, and he'll ask for another one.  
Give him another and you can ask about Baba Yaga, the Castle, and goblins.  
You don't really need info from him, but you get points for some preliminary 
questioning (2).

Now, head north to the Healer's Hut.  Knock on the door and question the 
Healer (2) about her potions, components, and her ring.  Head on outside.  
Note the nest in the tree.  Get some rocks from the ground by typing "get 
rocks".  "Throw rock" and you'll aim for the nest, and after a few throws, the 
nest will drop to the ground.  You'll find the ring (that was easy) (3).  Go 
back inside and give the ring to her.  She'll give you six golds and two 
Healing Potions for your reward (10).

Head west to the farm.  You'll find Heinrich the Centaur there.  Question 
him (1) about his name, his daughter, the farm, and the brigands.  Be 
absolutely sure to ask him about "leader" as well (3).

Go back east and north to the Castle Gates.  Question Karl the Gatekeeper (5), 
about the castle, the baron, Barnard, Elsa, Baba Yaga, and the curse.  Type 
"open the gate" and he'll open it for you.  Enter the Castle (1).

Once inside the Courtyard, you may chance to see the Weapon Master.  Question 
him (1) about fighting.  He'll ask you if you fight.  Say yes, and he'll ask 
if you want to train.  Say yes, and you'll give him a gold and fight against 
him in a side view battle (3).  Fight him, and you'll probably get exhausted 
before long.  Once you build up your skills a lot, (Stamina and Weapon Use at 
100 each) you can probably beat him.  Just attack repeatedly, and push him to 
the far end of the courtyard.  He'll give up, and walk off, never to 
return (10).

Also, you can head east to the Stables.  The Stable Hand will ask you to 
work.  Agree to it, and you'll do some good old fashioned raking (5).  You'll 
get five silvers for your troubles.

That's about all you can do for now in the outskirts of town.  You'll do some 
more later, but this is all the preliminary stuff.

---

Spielburg Forest:

If you're feeling up to it, you can finally head out into the wild woods and 
take out one or two monsters.  Make sure your stamina and health are high 
enough before you do any fighting.

You probably won't be able to fight everything right off the bat, but, as a 
Fighter, you'll get points for all the different monsters you kill.  

Goblin: 1
Purple Saurus: 1
Brigand: 1
Mantray: 2
Cheetaur: 4
Saurus Rex: 4
Troll: 4

Check the Monsters section for how and when to fight these guys.  Also, 
remember to get Claws from the Cheetaur, and the beard from a Troll.  Give 
them both to the Healer (2, 2).

Head up to Erana's Peace (start from the farm, go four screens north, one 
east, and one north).  This is the best place to sleep, as it will fully 
restore you.  Also, you can chow on the fruit trees (2) once a day to fully 
satisfy you and save on rations.  Grab a bunch of flowers while you're here.  
You can give them to the Healer (1).

Now, head towards the Mushroom Ring (start from the farm, go three screens 
west, and two south).  Grab some Magic Mushrooms to give to the Healer (1).

---

Other Points of Interest:

Head to the Snow Field (three screens east of the Healer's Hut).  You'll find 
Brauggi here.  He'll want a bargain of fruit.  If you hadn't done it yet, buy 
fifty apples from Hilde in town.  Give them to him, and you'll get the Glowing 
Gem (8).  Neato.

Head to Spiegelsee (three screens south of the town entrance).  Pause for 
some peaceful reflection (1).

Go to Flying Falls (two screens south of the town entrance and one east).  
You'll see a door on the cliffside.  Throw some rocks at the door.  Hit it 
three times and Henry the Hermit will walk out and make the ladder appear on 
his wall.  Climb up it.  Knock on the door (1).  Make sure you step to the 
right of the door so it doesn't knock you off the cliff.  You'll walk 
inside (5).  Have a seat and chat it up with Henry (2).  Ask him about the 
cave, magic, the brigands, and all related topics he brings up.

Go to Mount Zauberburg (one screen east of the Healer's Hut, then two screens 
north and two screens east).  Read the cute magic signs, then climb the 
mountain.  Once you reach the house, the Gargoyle above the house will ask 
you three questions.  Make sure you answer them correctly.  Here's the list of 
possible questions:

Name: Your name as you entered it (capitalization isn't important)
Quest: "Glory"
Fave Color: "Purple" (like the house)
Whose spell protects the town: "Erana"
Who do you seek: "Erasmus" or "Fenrus"
Baron's first name: "Stefan"
Thieves' Password: "I don't know" (No thieves allowed! ^_^)

Anyway, once inside (3), try not to fiddle with things.  Look all you want, 
just don't touch.  Head up to the tower.  Have a seat and you'll talk to 
Erasmus and Fenrus.  Ask them (1) about all the spellcasters in the valley.  
Ask them about Erana, Baba Yaga, Zara, the Warlock, Henry, and anything magic 
you might want to know about.  Also, ask him about curse and countercurse.  
"Stand up" once you're done and he'll send you to the bottom of the mountain.

At some point in your adventure, preferably after you built up your skills a 
bit, and have a decent cash flow, go to the Healer and purchase Undead Unguent 
for 100 silvers.  Head to Baba Yaga's Hut (from the farm, three screens west 
and two screens north).  Bonehead (the skull at the gates) will turn you away, 
unless you agree to deal with him.  "Ask about deal", then say "yes" (2).  
He'll want a glowing gem for his eyes.  Also, ask him about the hut and the 
rhyme (Hut of brown, now sit down).  If you have the Glowing Gem from the Snow 
Field, give it to Bonehead, and he'll lower the gate (10).  Type "hut of brown 
now sit down" and the house will squat (7).  Head on in (2).  Baba Yaga 
doesn't take too kindly to visitors, as you'll soon see.  When she asks you 
your name, tell her, then agree to help, and agree to the task she gives you, 
and she'll turn you loose.  Your mission now is to go to the graveyard and 
get some Mandrake, and you have to pull it at midnight (when it says "Middle 
of the Night" on the time thing).  Fight a bit and rest until it says "You 
are getting tired."  At that time, use the Undead Unguent (2) and head to the 
graveyard.  You'll see the ghosts playin' around.  Walk over to the little red 
plant growing out the grave and take it (6).  Once you snag it, run on back 
to Baba Yaga's and she'll take it from you (3), and you'll be off the hook.

---

Comes a Hero from the East:

Now that you've messed around a bit, it's time to do what you came here to 
do.  To start that, head to the Dryad's Tree in the southwest corner of the 
forest.  You'll follow a white stag in there.  Once you enter the area with a 
tree, the Dryad will pop out of her tree and ask if you're a friend of the 
forest.  Say "yes" (1) and she'll ask you to get a seed from the Spore 
Spitting Spirea in the north.

Find the Spore Spitting Spirea area (starting at the farm, go three screens 
north and one screen west).  Once you get there, throw some rocks, and you'll 
eventually connect with the seed they're tossing around.  Pick it up (7) and 
head back to the Dryad's Tree.  Say "yes" and you'll give her the seed (7).  
She'll give you the recipe for a Dispel Potion:

Flowers from Erana's Peace
Green Fur
Fairy Dust
Magic Acorn
Flying Water

Once she returns to her tree, she'll drop a Magic Acorn from her branches.  
Grab it (1).

Odds are you'll already have the Flowers turned in.

Green Fur: Head towards the Meep's Peep (from the farm, two screens west, one 
screen south, and three screens west).  Note the cute little meeps popping out 
of their holes.  "Talk to meeps" when one's popped up.  The Green Meep will 
jump out and talk to you.  Question him (1) about meeps, rocks, and fur.  Ask 
about green fur, and he'll give you some (5).

Flying Water: If you have an Empty Flask, go to Flying Falls.  Get some water 
from the falls (3).

Fairy Dust: Wait until night and go to the Mushroom Ring.  You'll see the 
cute little fairies dancing around.  They'll tell you to dance.  So agree to 
dance (3).  After you finish, ask them (1) about things (fairies, dryad, 
dust).  Once you ask them about dust, they'll give you some (8).  Make sure 
you have an empty flask to put it in.

Now that you have all five ingredients, go to the Healer's Hut.  Give her the 
Acorn (5), Green Fur (2), Fairy Dust (2), Flying Water (2), and the Flowers 
if you haven't already (1, if you haven't already).  She'll get to work on 
the Potion.  Leave and come back, and she'll give it to you (7).

---

Free the Man from in the Beast:

Okay, you should be all set to tackle some real quests, now.  You'll probably 
want your important skills at about 60 (Strength, Agility, Vitality, Weapon 
Use) or more.

You'll probably want some Healing and Vigor Potions for this.  Head to the 
Cave (from Erana's Peace, one screen south and one east) and you'll find an 
Ogre.  Kick his butt (2).  Once he's dead, "smash chest", and if your Strength 
is high enough, you'll force it open and get 1 gold and 43 silvers.

Heal up, and enter the cave.  You'll notice that rather nasty-looking bear 
chained to the floor by a manacle.  Don't fight him.  Give him a ration 
instead (or an apple or vegetable) (5) and you'll be able to walk past him.

Enter this new section of the cave (2) and you'll see a Kobold sitting with a 
key around his neck.  Fight the bugger.  Just keep stabbing until he 
dies (sinks into the ground) (10).  Pick up the key he leaves behind (7).  
Drink a Healing Potion and walk towards the bottom of the cave.  You'll bump 
into a chest.  "Smash chest" (BOOM!) and take the 6 golds and 10 silvers left 
behind (5).

Now, return to the bear and "unlock manacle".  The bear will turn into (Tah 
dah!) the Baronet, Barnard von Spielburg! (25) He'll act all snooty, and 
teleport back to the castle with his amulet.

Go to the castle.  Karl will let you in.  Walk up to the keep and they'll let 
you in to see the Baron (10).  The Baron will thank you himself for helping 
his son.  Ask him (3) about himself, his family, the brigands (leader and 
warlock), and the prophecy.  Leave the Great Hall and he'll ask you to stay 
the night and will give you the 50 gold reward.

---

Bring the Child from the Band:

Now it's time to take care of the Brigands.  But how?  Well, you could go to 
the Ambush down south, jump over the log, and fight your way in, but you won't 
get full credit.

Instead, go to town.  Go to the Dry Goods and buy the Chainmail Armor with 
your reward (3).  Go to the Tavern and you'll find a note under the 
stool (2, if you didn't get the first one).  Archery range at noon, eh?  Wait 
until the time says "midday" and head towards the Archery Range from the 
side (west side works best for me).  You'll see Bruno and Brutus (a brigand) 
discussing matters.  You'll learn about a secret entrance to the Fortress and 
the "secret word" (Hiden Goseke).  Bruno will take off (12).  Now, you need 
to get the key to that secret door.  If you have daggers, you can throw them 
at him to kill him, or you can go south and go back north right away (before 
Bruno spots you down there).  Once you get back north, you'll fight him.  Once 
he's dead, search him to get the key.  Leave the area and go south to where 
"the bouncer" hops around.  This is the Antwerp.  You can run around him so he 
doesn't bop you.  Once you get to the door, unlock it with the key, open 
it (10), and type "Hiden Goseke" (5).  This will cause Fred the Troll to 
retreat to the back of the cave.

Enter the cave (2).  The passage leads down to the bottom of the screen, 
but there's a side passage off to the left.  This leads to Fred's treasure 
hoard.  If you go for the hoard, though, you'll have to fight him, and he's 
REALLY tough.  A lot tougher than your average Troll.  If you beat him, take 
his beard, and search the pile.  You'll find 30 silvers and 5 gold.  (You 
don't get points for beating Fred or finding the hoard.)  Leave the cave by 
the southeast exit.

You'll come upon the Fortress Gate now.  You'll see a minotaur, Toro, 
guarding the area.  What, do I have to spell it out for you?  Get out 
there and kick his butt!  (Make sure you're healed first)  You won't kill him, 
but he will pass out from blood loss (5).  Search him for 50 silvers.  Bash 
the gate open with your incredible strength and you'll enter the fortress. (8)

Now, you'll be in the courtyard of the fortress.  Directly in front of you 
will be two blockades, and a rug in between them.  You can also go to the left 
or the right of the blockades.  Don't step on the rug, or go to the 
right (without, at least typing "step over rope" to pass the tripline 
stretched there).  Go to the left to be safe.  Now, you're in front of a pit.  
There are two bridges.  One has a sign that says "Cross Here".  Cross on the 
bridge on the right (the one with the sign).  Now, take a close look at the 
last area.  Note the grayish-black line.  That's a tripline.  Type "step over 
rope" and you can walk across.  Head through the back right door.  (8)

Now, you're in the fortress' main hall/mess hall.  Things will move really 
fast in here, so read carefully.  Do anything wrong, and you pretty much die, 
so save as you enter here.  Just as soon as you enter the room "close door".  
Walk up to the door on the upper right and the chair near it, but don't move 
it yet.  A few brigands will walk up behind you, note the closed door and 
leave.  As soon as the brigands start to leave, not before they leave, "move 
chair" to block the door.  Walk towards the nearby candelabra.  Now, three 
brigands (who look awfully familiar, nyuk nyuk) will enter the room and walk 
towards you from behind the table.  As soon as the first one walks behind the 
table, "move candelabra".  Oh, a wise guy, eh?  Now, walk in front of the 
table, quickly, and after the last one has passed the chair on the left, "jump 
on table".  The rest will soon be history.  Walk through the far door. (8)

This is an interesting room.  Here's the top-view layout:

                         7       5   
                        6|   ME  |                                 
                         }   }   |2                                
                       4 +---+---+                                 
                             }   |3                                
                        1----+-+-}                                 
                               |                                   
                          +----+                                   
                          |                                        
                          E                                      

You'll enter at point E.  Note that any "}" in the area is a trap door.  
If you walk over a trap door, or off any platform, you'll tumble out 
door 5 and keep rolling.  Type "stop" and you'll skid to a stop.

First things first.  Talk to ME.  This is the Brigand Warlock.  If you've 
been paying attention at all, you'd guess that this is Yorick, the Jester.  
Ask him (2) about Elsa or Yorick (8), and he'll take off to ready the secret 
passage.  Of course, if you don't bother to talk to him, he'll interfere with 
the process, tossing books, apple cores, etc. at you.  Best to let him go now.

Anyway, walk over to door 1 and you'll come out the doggie door at 2.  Enter 
door 3 and you'll come up at door 4 which is above the floor.  Pull the chain 
and you'll open 5.  Head back and go through 5.  You'll come out at 6.  Open 
door 7 and get out of the way (go halfway through door 6) before it flattens 
you.  Now, walk through door 7 and into the final room. (12)

Finally, you reach the Brigand Leader's room.  She'll hop over her desk to 
challenge you.  If you've figured anything out at all in this game, you'll 
know to throw the Dispel Potion at her.  Will our mystery guest enter and 
sign in please!  It's Elsa von Spielburg! (35)

Elsa and Yorick will take off and you'll be left in the room.  The brigands 
are trying to bash the door down.  Quick, grab the two Healing Potions and 
the Magic Mirror.  MAKE SURE YOU GRAB THE MIRROR! (10)

Now, leave the room by the passage Yorick used.

---

Drive the Curser from the Land:

It's time to settle the score with Baba Yaga.  You'll come out at the 
Antwerp's spot.  Head up to Baba Yaga's hut.  Bonehead will let you in.  Make 
the hut sit and head on in.  As soon as you enter, type "hold mirror", then 
walk in.  Baba Yaga will attempt to turn you into a frog again.  Heh heh. (50)

Anyway, she'll take off and you'll automatically head to Castle Spielburg for 
your reward ceremony. (25)

Congratulations!  You've proven yourself a hero!  If you've done everything, 
you should have 500 points!  Now, it's off to Shapeir in Quest for Glory II.

******************************************************************************
6. MAGIC USER WALKTHROUGH
******************************************************************************

Points added will be shown in parentheses.

As a Magic User, you'll be hitting the magic a lot, so be sure to practice 
all your spells, particularly Flame Dart, as it's your main combat spell.  
Your most important stats are Intelligence and Magic, but don't forget to 
beef your Vitality, Agility, and Strength, so you'll survive any chance 
encounters.

You'll start with a Dagger, Leather Armor, five Rations, four gold, ten 
silver, and the Zap Spell.

---

Town of Spielburg:

You'll stroll into town (1), take a short look around, and the Sheriff will 
greet you.  Question him for a bit (1).  First good place to head is to the 
Hero's Tale Inn.  You can question Shameen there (1).  Have a sit down and 
Shema will show up.  You can ask her a few things.  Order some food (1).  Be 
sure to ask one or both of them about Abdulla, to know that he'll show up 
later that night.

Leave the Inn and head west to the end of Main Street.  Enter the Magic Shop 
(y'know, the place with the big eye above it).  Meet and question Zara (1).  
You won't be able to afford Flame Dart yet, but you can buy Fetch or Open 
right now, and I suggest you buy one of them.  Anyway, for full points, 
you'll have to buy all three spells. (2, 2, 2)

Head to the Guild at the end of the screen.  Enter and read the Adventurer's 
Logbook (4).  Sign it, too (1).  Take a look at the Quest Board, too (6).  
Wake up the Guildmaster, Wolfgang.  Question him (1) about heroes, monsters, 
and the stuff on the Quest Board.  Question him about the "curse", too, and 
related topics.

Once you've finished with that, leave and go the other way on Main Street.  
You'll eventually reach Market Street.  Speak to Hilde the Centaur (1).  If 
you wish, you can buy 50 apples (3).  At five silvers total, you should 
easily be able to afford it, but it could weigh you down considerably.  Also, 
head into the Dry Goods Store and speak to Kaspar the proprietor (1).  Buy 
some food if you wish.  As you'll notice, you won't have enough to buy armor.  
Deal with it for now.  You'll have enough, eventually.

Head west to Tavern Street.  Head into the alley.  You'll find Sam begging on 
the street.  Give him a silver (1), and you'll be able to question him (1).  
Ask him about night and the brigands.  Also, ask some other topics related to 
what he brings up.

Walk into the Tavern.  Note the odd white thing next to the center stool.  
That's a note.  Pick up the note, and you'll learn some interesting things 
about two guys named "B" (2).  You can order some ale if you want, but don't 
drink three glasses of ale, or any Troll's Sweat (-5), or Dragon's Breath 
(too hot to handle).

That's about all there is to do inside town.  It's probably not Sunset yet, 
so you can go outside for a bit.  When Sunset rolls around, go to the inn, 
where you'll find Abdulla Doo lost in his own sorrows.  Have a seat.  You can 
order some food now if you haven't already.  Have a chat with Abdulla (5).  
Ask him about the brigands and his attack.  Order some food for him (when 
Shema's near) or give him a ration (2).  He'll tell you about his magic 
carpet and Shapeir, giving you some foreshadowing of your next quest.  Might 
want to deal with this one first.  Anyway, you probably should sleep at the 
Inn tonight (1).

The next morning, head out to start your adventure proper.

---

Outside the Town:

You're now outside of town, whistling to yourself (1).  Just as a quick note, 
be sure to always keep an eye on the time so you know when to bed down.

First order of business is to head down the road.  You'll eventually reach the 
end of the road, where a recent avalanche has blocked the pass.  No easy way 
out of this quest.  Head back along the road.  Walk back and forth between 
road screens and eventually you'll come upon a fox trapped near the road.  
He'll ask you to free him.  Do so, and he'll give you a piece of advice and 
info (10).

Head back towards the gate.  There's a chance there'll be a guy outside the 
gate.  This is Bruno, the seller of info.  You can try asking him things, but 
he'll want money first.  Give him a silver, and he'll ask for another one.  
Give him another and you can ask about Baba Yaga, the Castle, and goblins.  
You don't really need info from him, but you get points for some preliminary 
questioning (2).

Now, head north to the Healer's Hut.  Knock on the door and question the 
Healer (2) about her potions, components, and her ring.  Head on outside.  
Note the nest in the tree.  If you have the Fetch Spell, you can go right 
ahead and snag it.  You'll find the ring inside.  Go back inside and give the 
ring to her.  She'll give you six golds and two Healing Potions for your 
reward (10).  You can use it for some new spells from Zara.

Head west to the farm.  You'll find Heinrich the Centaur there.  Question 
him (1) about his name, his daughter, the farm, and the brigands.  Be 
absolutely sure to ask him about "leader" as well (3).

Go back east and north to the Castle Gates.  Question Karl the Gatekeeper (5), 
about the castle, the baron, Barnard, Elsa, Baba Yaga, and the curse.  Type 
"open gate" and he'll open it for you.  Enter the Castle (1).

Once inside the Courtyard, you may chance to see the Weapon Master.  Question 
him (1) about fighting.  Unfortunately, you can't fight him, since you're not 
a fighter.

Also, you can head east to the Stables.  The Stable Hand will ask you to 
work.  Agree to it, and you'll do some good old fashioned raking (5).  You'll 
get five silvers for your troubles.

That's about all you can do for now in the outskirts of town.  You'll do some 
more later, but this is all the preliminary stuff.

---

Spielburg Forest:

If you're feeling up to it, you can finally head out into the wild woods and 
take out one or two monsters.  Make sure your stamina and health are high 
enough before you do any fighting.

You probably won't be able to fight just anything right off the bat.  No more 
than a goblin or two per day.

Check the Monsters section for how and when to fight all the monsters.  Also, 
remember to get Claws from the Cheetaur, and the beard from a Troll.  Give 
them both to the Healer (2, 2).  Yeah, even though you don't get points for 
killing, you still have to fight the Cheetaur and Troll for the components.

Head up to Erana's Peace (start from the farm, go four screens north, one 
east, and one north).  This is the best place to sleep, as it will fully 
restore you.  Also, you can chow on the fruit trees (2) once a day to fully 
satisfy you and save on rations.  As a Magic User, you should sleep here at 
least once (5).  Also, read the runes on the stone.  Open it, eh?  If you 
have the Open Spell, cast it, and grab the Calm Spell inside (4).  Hmm.  
Looks like this isn't the final resting place of Erana.  Grab a bunch of 
flowers while you're here.  You can give them to the Healer (1).

Now, head towards the Mushroom Ring (start from the farm, go three screens 
west, and two south).  Grab some Magic Mushrooms to give to the Healer (1).

---

Other Points of Interest:

Head to the Snow Field (three screens east of the Healer's Hut).  You'll find 
Brauggi here.  He'll want a bargain of fruit.  If you hadn't done it yet, buy 
fifty apples from Hilde in town.  Give them to him, and you'll get the Glowing 
Gem (8).  Neato.

Head to Spiegelsee (three screens south of the town entrance).  Pause for 
some peaceful reflection (1).

Head off to the Meep's Peep in the west.  "Talk to meeps" and the green one 
will jump out and talk to you.  Ask him (1) about meeps and anything.  Ask 
him about magic and he'll give you the Detect Spell. (4)

Go to Flying Falls.  You'll see a door on the cliffside.  Cast Detect and the 
ladder to the door will appear.  Climb up it.  Knock on the door (1).  Make 
sure you step to the right of the door so it doesn't knock you off the cliff.  
You'll walk inside (5).  Have a seat and chat it up with Henry (2).  Ask him 
about the cave, magic, the brigands, and all related topics he brings up.  
When you ask about magic, be sure to ask about Trigger.  He'll give you the 
Trigger Spell. (4)

Go to Mount Zauberburg (one screen east of the Healer's Hut, then two screens 
north and two screens east).  Read the cute magic signs, then climb the 
mountain.  Once you reach the house, the Gargoyle above the house will ask 
you three questions.  Make sure you answer them correctly.  Here's the list of 
possible questions:

Name: Type your name (capitalization isn't necessary)
Quest: "Glory" or "Hero"
Fave Color: "Purple" (like the house)
Whose spell protects the town: "Erana"
Who do you seek: "Erasmus" or "Fenrus"
Baron's first name: "Stefan"
Thieves' Password: "I don't know" (No thieves allowed! ^_^)

Anyway, once inside (3), try not to fiddle with things.  Look all you want, 
just don't touch.  Head up to the tower.  Have a seat and you'll talk to 
Erasmus and Fenrus.  

He'll recognize you as a practitioner of magic and will want to play Mage's 
Maze with you.  You need Open, Fetch, Trigger, and Flame Dart to play.  If 
you have all those spells, agree to play (5):

So, you've got the playing field.  You start at the upper left with the white 
bug and Erasmus' is the purple bug in the upper right.  The bugs are 
self-willed, and there's not much you can do to get them moving in the 
direction you want to.  Just hope you get to the exit in the bottom right.

Use Fetch to move ladders and bridges around.  Click on the Fetch button and 
drag the thing you want to move to the spot you want.

Use Open to move boulders.  Click the button, then click the boulder to move 
it to a random location.

Use Flame Dart to create a miniature flame ball that attracts the creatures, 
or something.

Use Trigger to change the size of your bug.  You'll go from small to medium 
to large and back to small again.  Small creatures can pass through the small 
holes, and they move the fastest.  Medium creatures can climb ladders and eat 
small creatures, but they move slower.  Large creatures move the slowest, but 
they eat medium creatures, but not small creatures.  Like-sized creatures are 
generally attracted to each other.

So, do your best to get your bug there before Erasmus, and he'll teach you the 
Dazzle Spell (12).  If you fail, just keep trying once you restore your MP.

Once you've done playing, ask them (1) about all the spellcasters in the 
valley.  Ask them about Erana, Baba Yaga, Zara, the Warlock, Henry, and 
anything magic you might want to know about.  Also, ask him about curse and 
countercurse.  "Stand" once you're done and he'll send you to the bottom 
of the mountain.

At some point in your adventure, preferably after you built up your skills a 
bit, and have a decent cash flow, go to the Healer and purchase Undead Unguent 
for 100 silvers.  Head to Baba Yaga's Hut (from the farm, three screens west 
and two screens north).  Bonehead (the skull at the gates) will turn you away, 
unless you agree to deal with him.  "Ask about deal", then say "yes" (2).  
He'll want a glowing gem for his eyes.  Also, ask him about the hut and the 
rhyme (Hut of brown, now sit down).  If you have the Glowing Gem from the Snow 
Field, give it to Bonehead, and he'll lower the gate (10).  Type "hut of brown 
now sit down" and the house will squat (7).  Head on in (2).  Baba Yaga 
doesn't take too kindly to visitors, as you'll soon see.  When she asks you 
your name, tell her, then agree to help, and agree to the task she gives you, 
and she'll turn you loose.  Your mission now is to go to the graveyard and 
get some Mandrake, and you have to pull it at midnight (when it says "Middle 
of the Night" on the time thing).  Fight a bit and rest until it says "You 
are getting tired."  At that time, use the Undead Unguent (2) and head to the 
graveyard.  You'll see the ghosts playin' around.  Walk over to the little red 
plant growing out the grave and take it (6).  Once you snag it, run on back 
to Baba Yaga's and she'll take it from you (3), and you'll be off the hook.

---

Comes a Hero from the East:

Now that you've messed around a bit, it's time to do what you came here to 
do.  To start that, head to the Dryad's Tree in the southwest corner of the 
forest.  You'll follow a white stag in there.  Once you enter the area with a 
tree, the Dryad will pop out of her tree and ask if you're a friend of the 
forest.  Say "yes" (1) and she'll ask you to get a seed from the Spore 
Spitting Spirea in the north.

Find the Spore Spitting Spirea area (start at the farm, then go three screens 
north and one screen west).  Once you get there, cast a Fetch Spell, and you 
should snag the seed they're tossing around.  Pick it up (7) and head back to 
the Dryad's Tree.  Say "yes" and you'll give her the seed (7).  She'll give 
you the recipe for a Dispel Potion:

Flowers from Erana's Peace
Green Fur
Fairy Dust
Magic Acorn
Flying Water

Once she returns to her tree, she'll drop a Magic Acorn from her branches.  
Grab it (1).

Odds are you'll already have the Flowers turned in.

Green Fur: Return to the Meep's Peep.  Ask about green fur, and he'll give 
you some (5).

Flying Water: If you have an Empty Flask, go to Flying Falls.  Get some 
water from the falls (3).

Fairy Dust: Wait until night and go to the Mushroom Ring.  You'll see the 
cute little fairies dancing around.  They'll tell you to dance.  So agree to 
dance (3).  After you finish, ask them (1) about things (fairies, dryad, 
dust).  Once you ask them about dust, they'll give you some (8).  Make sure 
you have an empty flask to put it in.

Now that you have all five ingredients, go to the Healer's Hut.  Give her the 
Acorn (5), Green Fur (2), Fairy Dust (2), Flying Water (2), and the Flowers 
if you haven't already (1, if you haven't already).  She'll get to work on 
the Potion.  Leave and come back, and she'll give it to you (7).

---

Free the Man from in the Beast:

Okay, you should be all set to tackle some real quests, now.  You'll probably 
want your important skills at about 60 (Intelligence, Magic, and the skill in 
Flame Dart) or more.

You'll probably want some Healing, Vigor, and Magic Power Potions for this.  
Head to the Cave (from Erana's Peace, one screen south and one east) and 
you'll find an Ogre.  Kick his butt if you want.  Once he's dead, cast Open, 
and if your skill is high enough, you'll open it and get 1 gold and 
43 silvers.  If you don't want to, just cast Calm and run past him.

Heal up, and enter the cave.  You'll notice that rather nasty-looking bear 
chained to the floor by a manacle.  Don't fight him.  Give him a ration 
instead (or an apple or vegetable) (5) and you'll be able to walk past him.  
You can even Calm it if you want, which will also give you the points.

Enter this new section of the cave (2) and you'll see a Kobold sitting with a 
key around his neck.  This is your first real magical duel, although it's more 
brute force than anything.  Cast a bunch of Flame Darts at him to kill him.  
He'll counter with Force Bolt and Reversal (new spells you'll learn in the 
next game).  Keep pounding, taking potions as necessary, and he'll sink into 
the ground, dead (10).  Pick up the key he leaves behind (7).  Cast Detect 
and you'll find a chest near the bottom of the cave.  Cast Trigger or Open a 
safe distance away (BOOM!) and take the 6 golds and 10 silvers left 
behind (5).

Now, return to the bear and "unlock manacle".  The bear will turn into (Tah 
dah!) the Baronet, Barnard von Spielburg! (25) He'll act all snooty, and 
teleport back to the castle with his amulet.

Go to the castle.  Karl will let you in.  Walk up to the keep and they'll let
you in to see the Baron (10).  The Baron will thank you himself for helping 
his son.  Ask him (3) about himself, his family, the brigands (leader and 
warlock), and the prophecy.  Leave the Great Hall and he'll ask you to stay 
the night and will give you the 50 gold reward.

---

Bring the Child from the Band:

Now it's time to take care of the Brigands.  But how?  Well, you could go to 
the Ambush down south, jump over the log, and fight your way in, but you won't 
get full credit, and you'll get hurt a lot.

Instead, go to town.  Go to the Tavern and you'll find a note under the 
stool (2, if you didn't get the first one).  Archery range at noon, eh?  Wait 
until the time says "midday" and head towards the Archery Range from the west 
side.  You'll see Bruno and Brutus (a brigand) discussing matters.  You'll 
learn about a secret entrance to the Fortress and the "secret word" (Hiden 
Goseke).  Bruno will take off (12).  Now, you need to get the key to that 
secret door.  Cast a bunch of Flame Darts at him to kill him, or you can go 
south and go back north right away (before Bruno spots you down there).  Once 
you get back north, you'll fight him.  Once he's dead, search him to get the 
key.  Leave the area and go south to where "the bouncer" hops around.  This 
is the Antwerp.  You can run around him so he doesn't bop you.  Once you get 
to the door, unlock it with the key, open it (10), and type "Hiden 
Goseke" (5).  This will cause Fred the Troll to retreat to the back of the 
cave.

Enter the cave (2).  The passage leads down to the bottom of the screen, 
but there's a side passage off to the left.  This leads to Fred's treasure 
hoard.  If you go for the hoard, though, you'll have to fight him, and he's 
REALLY tough.  A lot tougher than your average Troll.  If you beat him, take 
his beard, and search the pile.  You'll find 30 silvers and 5 gold.  (You 
don't get points for beating Fred or finding the hoard.)  Leave the cave by 
the southeast exit.

You'll come upon the Fortress Gate now.  You'll see a minotaur, Toro, guarding 
the area.  Cast Calm to put him to sleep.  Now, walk out and cast Open on the 
door and you'll enter the fortress. (8)

Now, you'll be in the courtyard of the fortress.  Directly in front of you 
will be two blockades, and a rug in between them.  You can also go to the left 
or the right of the blockades.  Don't step on the rug, or go to the 
right (without, at least typing "step over rope" to pass the tripline 
stretched there).  Go to the left to be safe.  Now, you're in front of a pit.  
There are two bridges.  One has a sign that says "Cross Here".  Cross on the 
bridge on the right (the one with the sign).  Now, take a close look at the 
last area.  Note the grayish-black line.  That's a tripline.  Type "step over 
rope" and you can walk across.  Head through the back right door.  (8)

Now, you're in the fortress' main hall/mess hall.  Things will move really 
fast in here, so read carefully.  Do anything wrong, and you pretty much die, 
so save as you enter here.  Just as soon as you enter the room "close door".  
Walk up to the door on the upper right and the chair near it, but don't move 
it yet.  A few brigands will walk up behind you, note the closed door and 
leave.  As soon as the brigands start to leave, not before they leave, "move 
chair" to block the door.  Walk towards the nearby candelabra.  Now, three 
brigands (who look awfully familiar, nyuk nyuk) will enter the room and walk 
towards you from behind the table.  As soon as the first one walks behind the 
table, "move candelabra".  Oh, a wise guy, eh?  Now, walk in front of the 
table, quickly, and after the last one has passed the chair on the left, "jump 
on table".  The rest will soon be history.  Walk through the far door. (8)

This is an interesting room.  Here's the top-view layout:

                         7       5   
                        6|   ME  |                                 
                         }   }   |2                                
                       4 +---+---+                                 
                             }   |3                                
                        1----+-+-}                                 
                               |                                   
                          +----+                                   
                          |                                        
                          E                                      

You'll enter at point E.  Note that any "}" in the area is a trap door.  
If you walk over a trap door, or off any platform, you'll tumble out 
door 5 and keep rolling.  Type "stop" and you'll skid to a stop.

First things first.  Talk to ME.  This is the Brigand Warlock.  If you've 
been paying attention at all, you'd guess that this is Yorick, the Jester.  
Ask him (2) about Elsa or Yorick (8), and he'll take off to ready the secret 
passage.  Of course, if you don't bother to talk to him, he'll interfere with 
the process, tossing books, apple cores, etc. at you.  Best to let him go now.

Anyway, walk over to door 1 and you'll come out the doggie door at 2.  Enter 
door 3 and you'll come up at door 4 which is above the floor.  Pull the chain 
and you'll open 5.  Head back and go through 5.  You'll come out at 6.  Open 
door 7 and get out of the way (go halfway through door 6) before it flattens 
you.  Now, walk through door 7 and into the final room. (12)

Finally, you reach the Brigand Leader's room.  She'll hop over her desk to 
challenge you.  If you've figured anything out at all in this game, you'll 
know to throw the Dispel Potion at her.  Will our mystery guest enter and 
sign in please!  It's Elsa von Spielburg! (35)

Elsa and Yorick will take off and you'll be left in the room.  The brigands 
are trying to bash the door down.  Quick, grab the two Healing Potions and 
the Magic Mirror.  MAKE SURE YOU GRAB THE MIRROR! (10)

Now, leave the room by the passage Yorick used.

---

Drive the Curser from the Land:

It's time to settle the score with Baba Yaga.  You'll come out at the 
Antwerp's spot.  Head up to Baba Yaga's hut.  Bonehead will let you in.  Make 
the hut sit and head on in.  As soon as you enter, type "hold mirror", then 
walk in.  Baba Yaga will attempt to turn you into a frog again.  Heh heh. (50)

Anyway, she'll take off and you'll automatically head to Castle Spielburg for 
your reward ceremony. (25)

Congratulations!  You've proven yourself a hero!  If you've done everything, 
you should have 500 points!  Now, it's off to Shapeir in Quest for Glory II.

******************************************************************************
7. THIEF WALKTHROUGH
******************************************************************************

Points added will be shown in parentheses.

As a Thief, you'll be having a lot of fun robbing the people you're trying to 
help.  To this end, build up your Agility, Vitality, and all your cute little 
Thief skills as much as possible.  In particular, make sure you Sneak 
everywhere.  Good sneaking skills can avoid fights, and prevent you from 
getting caught at inopportune times.

You'll start with a dagger, a Lock Pick, Leather Armor, five Rations, four 
gold, and 10 silver.

---

Town of Spielburg:

You'll stroll into town (1), take a short look around, and the Sheriff will 
greet you.  Question him for a bit (1).  First good place to head is to the 
Hero's Tale Inn.  You can question Shameen there (1).  Have a sit down and 
Shema will show up.  You can ask her a few things.  Order some food (1).  Be 
sure to ask one or both of them about Abdulla, to know that he'll show up 
later that night.

Leave the Inn and head west to the end of Main Street.  Enter the Magic Shop 
(y'know, the place with the big eye above it).  Meet and question Zara (1).  
Since you're not a practitioner, all you're really here for is the point you 
get for questioning her.  Leave the Magic Shop.

Head to the Guild at the end of the screen.  Enter and read the Adventurer's 
Logbook (4).  Sign it, too (1).  Take a look at the Quest Board, too (6).  
Wake up the Guildmaster, Wolfgang.  Question him (1) about heroes, monsters, 
and the stuff on the Quest Board.  Question him about the "curse", too, and 
related topics.

Once you've finished with that, leave and go the other way on Main Street.  
You'll eventually reach Market Street.  Speak to Hilde the Centaur (1).  If 
you wish, you can buy 50 apples (3).  At five silvers total, you should 
easily be able to afford it, but it could weigh you down considerably.  Also, 
head into the Dry Goods Store and speak to Kaspar the proprietor (1).  Buy 
some food if you wish.  You'll also want to use this store to build up your 
stock of daggers.  I suggest having at least five before the game's out, but 
don't bother buying more just yet.

Head west to Tavern Street.  Head into the alley.  You'll find Sam begging on 
the street.  Give him a silver (1), and you'll be able to question him (1).  
Ask him about night and the brigands.  Also, ask some other topics related to 
what he brings up.

Walk into the Tavern.  Note the odd white thing next to the center stool.  
That's a note.  Pick up the note, and you'll learn some interesting things 
about two guys named "B" (2).  You can order some ale if you want, but don't 
drink three glasses of ale, or any Troll's Sweat (-5), or Dragon's Breath 
(too hot to handle).

That's about all there is to do inside town.  It's probably not Sunset yet, 
so you can go outside for a bit.  When Sunset rolls around, go to the inn, 
where you'll find Abdulla Doo lost in his own sorrows.  Have a seat.  You can 
order some food now if you haven't already.  Have a chat with Abdulla (5).  
Ask him about the brigands and his attack.  Order some food for him (when 
Shema's near) or give him a ration (2).  He'll tell you about his magic carpet 
and Shapeir, giving you some foreshadowing of your next quest.  Might want to 
deal with this one first.  Anyway, you probably should sleep at the Inn 
tonight (1).

Of course, that's all there was to do at the daytime.  After hours, of course, 
is a different story altogether, particularly for one of your talents.

The very first thing you'll want to do is wait until dark, and go into the 
alley at night.  You'll notice a flashing coin at the end.  Head towards it, 
and Sneak and Slink will jump out at you.  "Make thief sign" and he'll tell 
you to give the password to Crusher in the Tavern (3).  Go into the Tavern 
and tell him the password and he'll let you into the Thieves' Guild. (5)  
Hooray. ^_^

Inside the Guild, the Chief will rant a bit about amateurs, and he'll tell you 
it'll cost 25 silvers to work in the town.  You should have the cash, so 
cough it up, and you'll get your Thieves' Guild License, Local 1313. (3)

Now, you'll want to build up your Lockpicking skill considerably before you 
try to rob any houses.  You have two potential marks: The Little Old Lady's 
house and the Sheriff's house.  Both have their elements of danger, so save 
often.

---

Little Old Lady's House:

Definitely the easier of the two.  Pick the lock on the door (5) and you'll 
enter a musty, smelly old house.  You'll note the cat wandering around.  
Search the desk at the left wall (1), search the couch (1), open the 
purse on the couch (1), check the basket for the string of pearls (1), and 
take the silver candlesticks (1) on the table.  Also, feed or pet the 
kitty (3).  Whatever you do, don't act aggressive towards the cat, or go 
upstairs.  You can, however, play with the cage for a little giggle.

---

Sheriff's House:

More risk involved here.  Pick the lock on the door (5) and you'll enter a 
relatively well-kept place.  Take the candelabra off the desk (1).  Take 
the silver in the desk.  You can open the music box on the table for 
another chuckle.  Take it afterwards (1).  Note the vase on the mantle.  You 
need to take or move the vase to get behind the painting, otherwise you'll 
break it, and that would make lots of unwanted noise.  Take the vase (1), and 
move the painting (1).  Open the safe (1), and take the bag of silver 
inside (1).

That's about all the true thievery you can do.  All the rest of it is more 
quest-related.

Go back to the Thieves' Guild.  Fence any stolen goods you have to Boris (3).  
If you have enough, be sure to buy a Thieves' Tool Kit (3).  Also, you can 
play Dag-Nab-It with the Chief for some extra cash. (3)

The rules to Dag-Nab-It are simple.  Score more than the Chief by throwing 
well.  Adjust your angle with the left and right arrow keys and adjust your 
force with the up and down arrow keys.  Press Enter to throw.  Wager however 
much you want.  Wager at least one game at more than 25 silvers and win (5).

The next morning, head out to start your adventure proper.

---

Outside the Town:

You're now outside of town, whistling to yourself (1).  Just as a quick note, 
be sure to always keep an eye on the time so you know when to bed down.

First order of business is to head down the road.  You'll eventually reach the 
end of the road, where a recent avalanche has blocked the pass.  No easy way 
out of this quest.  Head back along the road.  Walk back and forth between 
road screens and eventually you'll come upon a fox trapped near the road.  
He'll ask you to free him.  Do so, and he'll give you a piece of advice and 
info (10).

Head back towards the gate.  There's a chance there'll be a guy outside the 
gate.  This is Bruno, the seller of info.  You can try asking him things, but 
he'll want money first.  Give him a silver, and he'll ask for another one.  
Give him another and you can ask about Baba Yaga, the Castle, and goblins.  
You don't really need info from him, but you get points for some preliminary 
questioning (2).  You can also use the Thief Sign on him and he'll give you 
the REAL scoop on the Guild.

Now, head north to the Healer's Hut.  Knock on the door and question the 
Healer (2) about her potions, components, and her ring.  Head on outside.  
Note the nest in the tree.  "Climb tree" and you'll shimmy up the bark if your 
skill is high enough.  If it isn't, just keep trying and it'll get there.  
Once you are up high enough, click the Hand on the nest.  You'll find the 
ring (that was easy) (3).  Go back inside and give the ring to her.  She'll 
give you six golds and two Healing Potions for your reward (10).

Head west to the farm.  You'll find Heinrich the Centaur there.  Question 
him (1) about his name, his daughter, the farm, and the brigands.  Be 
absolutely sure to ask him about "leader" as well (3).

Go back east and north to the Castle Gates.  Question Karl the Gatekeeper (5), 
about the castle, the baron, Barnard, Elsa, Baba Yaga, and the curse.  Type 
"open gate" and he'll open it for you.  Enter the Castle (1).

Once inside the Courtyard, you may chance to see the Weapon Master.  Question 
him (1) about fighting.  Unfortunately, you can't fight him, since you're not 
a fighter.

Also, you can head east to the Stables.  The Stable Hand will ask you to 
work.  Agree to it, and you'll do some good old fashioned raking (5).  You'll 
get five silvers for your troubles.

That's about all you can do for now in the outskirts of town.  You'll do some 
more later, but this is all the preliminary stuff.

---

Spielburg Forest:

If you're feeling up to it, you can finally head out into the wild woods and 
take out one or two monsters.  Make sure your stamina and health are high 
enough before you do any fighting.

You probably won't be able to fight just anything right off the bat.  No more 
than a goblin or two per day.

Check the Monsters section for how and when to fight all the monsters.  Also, 
remember to get Claws from the Cheetaur, and the beard from a Troll.  Give 
them both to the Healer (2, 2).  Yeah, even though you don't get points for 
killing, you still have to fight the Cheetaur and Troll for the components.

Head up to Erana's Peace (start from the farm, go four screens north, one 
east, and one north).  This is the best place to sleep, as it will fully 
restore you.  Also, you can chow on the fruit trees (2) once a day to fully 
satisfy you and save on rations.  Grab a bunch of flowers while you're here.  
You can give them to the Healer (1).

Now, head towards the Mushroom Ring (start from the farm, go three screens 
west, and two south).  Grab some Magic Mushrooms to give to the Healer (1).

---

Other Points of Interest:

Head to the Snow Field (three screens east of the Healer's Hut).  You'll find 
Brauggi here.  He'll want a bargain of fruit.  If you hadn't done it yet, buy 
fifty apples from Hilde in town.  Give them to him, and you'll get the Glowing 
Gem (8).  Neato.

Head to Spiegelsee (three screens south of the town entrance).  Pause for 
some peaceful reflection (1).

Go to Flying Falls.  You'll see a door on the cliffside.  Try to climb up the 
wall.  Once you're high enough in Climbing skill, you'll shimmy right up.  
Knock on the door (1).  Make sure you step to the right of the door so it 
doesn't knock you off the cliff.  You'll walk inside (5).  Have a seat and 
chat it up with Henry (2).  Ask him about the cave, magic, the brigands, and 
all related topics he brings up.

Go to Mount Zauberburg (one screen east of the Healer's Hut, then two screens 
north and two screens east).  Read the cute magic signs, then climb the 
mountain.  Once you reach the house, the Gargoyle above the house will ask 
you three questions.  Make sure you answer them correctly.  Here's the list of 
possible questions:

Name: Type your name (capitalization)
Quest: "Glory" or "Hero"
Fave Color: "Purple" (like the house) or "Red and Black" (like your outfit)
Whose spell protects the town: "Erana"
Who do you seek: "Erasmus" or "Fenrus"
Baron's first name: "Stefan"
Thieves' Password: "I don't know" (No thieves allowed! ^_^)

Anyway, once inside (3), try not to fiddle with things.  Look all you want, 
just don't touch.  Head up to the tower.  Have a seat and you'll talk to 
Erasmus and Fenrus.  Ask them (1) about all the spellcasters in the valley.  
Ask them about Erana, Baba Yaga, Zara, the Warlock, Henry, and anything magic 
you might want to know about.  Also, ask him about curse and countercurse.  
"Stand" once you're done and he'll send you to the bottom of the mountain.

At some point in your adventure, preferably after you built up your skills a 
bit, and have a decent cash flow, go to the Healer and purchase Undead Unguent 
for 100 silvers.  Head to Baba Yaga's Hut (from the farm, three screens west 
and two screens north).  Bonehead (the skull at the gates) will turn you away, 
unless you agree to deal with him.  "Ask about deal", then say "yes" (2).  
He'll want a glowing gem for his eyes.  Also, ask him about the hut and the 
rhyme (Hut of brown, now sit down).  If you have the Glowing Gem from the Snow 
Field, give it to Bonehead, and he'll lower the gate (10).  Type "hut of brown 
now sit down" and the house will squat (7).  Head on in (2).  Baba Yaga 
doesn't take too kindly to visitors, as you'll soon see.  When she asks you 
your name, tell her, then agree to help, and agree to the task she gives you, 
and she'll turn you loose.  Your mission now is to go to the graveyard and 
get some Mandrake, and you have to pull it at midnight (when it says "Middle 
of the Night" on the time thing).  Fight a bit and rest until it says "You 
are getting tired."  At that time, use the Undead Unguent (2) and head to the 
graveyard.  You'll see the ghosts playin' around.  Walk over to the little red 
plant growing out the grave and take it (6).  Once you snag it, run on back 
to Baba Yaga's and she'll take it from you (3), and you'll be off the hook.


---

Comes a Hero from the East:

Now that you've messed around a bit, it's time to do what you came here to 
do.  To start that, head to the Dryad's Tree in the southwest corner of the 
forest.  You'll follow a white stag in there.  Once you enter the area with a 
tree, the Dryad will pop out of her tree and ask if you're a friend of the 
forest.  Say "yes" (1) and she'll ask you to get a seed from the Spore 
Spitting Spirea in the north.

Find the Spore Spitting Spirea area on the map (SSS).  Once you get 
there, throw some rocks, and you'll eventually connect with the seed 
they're tossing around.  Pick it up (7) and head back to the Dryad's 
Tree.  Say "yes" and you'll give her the seed (7).  She'll give you the 
recipe for a Dispel Potion:

Flowers from Erana's Peace
Green Fur
Fairy Dust
Magic Acorn
Flying Water

Once she returns to her tree, she'll drop a Magic Acorn from her branches.  
Grab it (1).

Odds are you'll already have the Flowers turned in.

Green Fur: Head towards the Meep's Peep (from the farm, two screens west, one 
screen south, and three screens west).  Note the cute little meeps popping out 
of their holes.  "Talk to meeps" when one's popped up.  The Green Meep will 
jump out and talk to you.  Question him (1) about meeps, rocks, and fur.  Ask 
about green fur, and he'll give you some (5).

Flying Water: If you have an Empty Flask, go to Flying Falls.  Get some water 
from the falls (3).

Fairy Dust: Wait until night and go to the Mushroom Ring.  You'll see the 
cute little fairies dancing around.  They'll tell you to dance.  So agree to 
dance (3).  After you finish, ask them (1) about things (fairies, dryad, 
dust).  Once you ask them about dust, they'll give you some (8).  Make sure 
you have an empty flask to put it in.

Now that you have all five ingredients, go to the Healer's Hut.  Give her the 
Acorn (5), Green Fur (2), Fairy Dust (2), Flying Water (2), and the Flowers 
if you haven't already (1, if you haven't already).  She'll get to work on 
the Potion.  Leave and come back, and she'll give it to you (7).

---

Free the Man from in the Beast:

Okay, you should be all set to tackle some real quests, now.  You'll probably 
want your important skills at about 60 (Agility, Vitality, Weapon Use, 
Stealth, Lockpicking) or more.

You'll probably want some Healing and Vigor Potions for this.  Head to 
the Cave (from Erana's Peace, one screen south and one east) and you'll find 
an Ogre.  Kick his butt if you want to.  You may be able to run past him if 
you're lucky.  If you decide to kill him, and you succeed, Use your Lockpick 
or Toolkit on his chest, and if your Lockpicking skill is high enough, you'll 
spring it open and get 1 gold and 43 silvers.

Heal up, and enter the cave.  You'll notice that rather nasty-looking bear 
chained to the floor by a manacle.  Don't fight him.  Give him a ration 
instead (or an apple or vegetable) (5) and you'll be able to walk past him.

Enter this new section of the cave (2) and you'll see a Kobold sitting with a 
key around his neck.  Don't even bother messing with him.  Just sneak up and 
lift the key right off him (7).  Walk towards the bottom of the cave.  You'll 
bump into a chest.  Pick the lock on the chest and pray your skill is high 
enough.  Get it open and take the 6 golds and 10 silvers left behind (5).

Now, return to the bear and "unlock manacle".  The bear will turn into (Tah 
dah!) the Baronet, Barnard von Spielburg! (25) He'll act all snooty, and 
teleport back to the castle with his amulet.

Go to the castle.  Karl will let you in.  Walk up to the keep and they'll let 
you in to see the Baron (10).  The Baron will thank you himself for helping 
his son.  Ask him (3) about himself, his family, the brigands (leader and 
warlock), and the prophecy.  Leave the Great Hall and he'll ask you to stay 
the night and will give you the 50 gold reward.

---

Bring the Child from the Band:

Now it's time to take care of the Brigands.  But how?  Well, you could go to 
the Ambush down south, jump over the log, and fight your way in, but you won't 
get full credit.

Instead, go to town.  Go to the Dry Goods and buy the Chainmail Armor with 
your reward (3).  Go to the Tavern and you'll find a note under the 
stool (2, if you didn't get the first one).  Archery range at noon, eh?  Wait 
until the time says "midday" and head towards the Archery Range from the 
side (west side works best for me).  You'll see Bruno and Brutus (a brigand) 
discussing matters.  You'll learn about a secret entrance to the Fortress and 
the "secret word" (Hiden Goseke).  Bruno will take off (12).  Now, you need 
to get the key to that secret door.  If you have daggers, you can throw them 
at him to kill him, or you can go south and go back north right away (before 
Bruno spots you down there).  Once you get back north, you'll fight him.  Once 
he's dead, search him to get the key.  Leave the area and go south to where 
"the bouncer" hops around.  This is the Antwerp.  You can run around him so he 
doesn't bop you.  Once you get to the door, unlock it with the key, open 
it (10), and type "Hiden Goseke" (5).  This will cause Fred the Troll to 
retreat to the back of the cave.

Enter the cave (2).  The passage leads down to the bottom of the screen, 
but there's a side passage off to the left.  This leads to Fred's treasure 
hoard.  If you go for the hoard, though, you'll have to fight him, and he's 
REALLY tough.  A lot tougher than your average Troll.  If you beat him, take 
his beard, and search the pile.  You'll find 30 silvers and 5 gold.  (You 
don't get points for beating Fred or finding the hoard.)  Leave the cave by 
the southeast exit.

You'll come upon the Fortress Gate now.  You'll see a minotaur, Toro, 
guarding the area.  Get sneaking and sneak past that bush.  Toro doesn't have 
good peripheral vision, so moving silently is good enough.  Climb over the 
gate and enter the fortress. (8)

Now, you'll be in the courtyard of the fortress.  Directly in front of you 
will be two blockades, and a rug in between them.  You can also go to the left 
or the right of the blockades.  Don't step on the rug, or go to the 
right (without, at least typing "step over rope" to pass the tripline 
stretched there).  Go to the left to be safe.  Now, you're in front of a pit.  
There are two bridges.  One has a sign that says "Cross Here".  Cross on the 
bridge on the right (the one with the sign).  Now, take a close look at the 
last area.  Note the grayish-black line.  That's a tripline.  Type "step over 
rope" and you can walk across.  Head through the back right door.  (8)

Now, you're in the fortress' main hall/mess hall.  Things will move really 
fast in here, so read carefully.  Do anything wrong, and you pretty much die, 
so save as you enter here.  Just as soon as you enter the room "close door".  
Walk up to the door on the upper right and the chair near it, but don't move 
it yet.  A few brigands will walk up behind you, note the closed door and 
leave.  As soon as the brigands start to leave, not before they leave, "move 
chair" to block the door.  Walk towards the nearby candelabra.  Now, three 
brigands (who look awfully familiar, nyuk nyuk) will enter the room and walk 
towards you from behind the table.  As soon as the first one walks behind the 
table, "move candelabra".  Oh, a wise guy, eh?  Now, walk in front of the 
table, quickly, and after the last one has passed the chair on the left, "jump 
on table".  The rest will soon be history.  Walk through the far door. (8)

This is an interesting room.  Here's the top-view layout:

                         7       5   
                        6|   ME  |                                 
                         }   }   |2                                
                       4 +---+---+                                 
                             }   |3                                
                        1----+-+-}                                 
                               |                                   
                          +----+                                   
                          |                                        
                          E                                      

You'll enter at point E.  Note that any "}" in the area is a trap door.  
If you walk over a trap door, or off any platform, you'll tumble out 
door 5 and keep rolling.  Type "stop" and you'll skid to a stop.

First things first.  Talk to ME.  This is the Brigand Warlock.  If you've 
been paying attention at all, you'd guess that this is Yorick, the Jester.  
Ask him (2) about Elsa or Yorick (8), and he'll take off to ready the secret 
passage.  Of course, if you don't bother to talk to him, he'll interfere with 
the process, tossing books, apple cores, etc. at you.  Best to let him go now.

Anyway, walk over to door 1 and you'll come out the doggie door at 2.  Enter 
door 3 and you'll come up at door 4 which is above the floor.  Pull the chain 
and you'll open 5.  Head back and go through 5.  You'll come out at 6.  Open 
door 7 and get out of the way (go halfway through door 6) before it flattens 
you.  Now, walk through door 7 and into the final room. (12)

Finally, you reach the Brigand Leader's room.  She'll hop over her desk to 
challenge you.  If you've figured anything out at all in this game, you'll 
know to throw the Dispel Potion at her.  Will our mystery guest enter and 
sign in please!  It's Elsa von Spielburg! (35)

Elsa and Yorick will take off and you'll be left in the room.  The brigands 
are trying to bash the door down.  Quick, grab the two Healing Potions and 
the Magic Mirror.  MAKE SURE YOU GRAB THE MIRROR! (10)

Now, leave the room by the passage Yorick used.

---

Drive the Curser from the Land:

It's time to settle the score with Baba Yaga.  You'll come out at the 
Antwerp's spot.  Head up to Baba Yaga's hut.  Bonehead will let you in.  Make 
the hut sit and head on in.  As soon as you enter, type "hold mirror", then 
walk in.  Baba Yaga will attempt to turn you into a frog again.  Heh heh. (50)

Anyway, she'll take off and you'll automatically head to Castle Spielburg for 
your reward ceremony. (25)

Congratulations!  You've proven yourself a hero!  If you've done everything, 
you should have 500 points!  Now, it's off to Shapeir in Quest for Glory II.

******************************************************************************
8. THE WHOLE STORY AND EPILOGUE
******************************************************************************

Ten years ago, Baba Yaga entered Spielburg Valley.  Stefan von Spielburg, 
realizing the threat she stood, ordered her to leave.  She responded by 
cursing the graveyard, so that the dead walk at night.

Afterwards, the Baron sent his contingent of guards to eliminate her, or at 
least force her to leave.  It would not be nearly as simple as that.  She then 
cursed the Baron himself, declaring that he would lose all he treasures.  His 
guard force was severly weakened from the attack.

Soon afterwards, a great flying beast (most probably the hut itself) swooped 
over the castle wall and stole away the eight year old Elsa von Spielburg.  
Baba Yaga enchanted Elsa to forget her name and to believe she was a brigand's 
child.  She gave her to the brigands, who raised her as one of their own.

Two years afterwards, Yorick, the Jester, who swore he'd never give up looking 
for her, found her in her state.  He posed as the Warlock for the Brigands, 
with his knowledge of sleight of hand.

Over the next six years, Elsa would distinguish herself from the rest of the 
brigands with her skill in swordsmanship and thievery.  At the end of those 
six years, she took over the band as their leader.  For the next two years, 
the brigands would become a very formidable force, and would often rob 
travellers.

Also, five years ago, the Baronet, Barnard von Spielburg, went hunting one 
day.  He found the Kobold Cave.  The Kobold ordered him to leave, but he 
refused, and so the Kobold changed him into a bear.

So, at present day, the valley is in a sorry state.  Perfect time for a hero 
to show up.

And so he does, and saves the valley as only a hero can.

Afterwards, the Hero joined Abdulla Doo and the Kattas and took flight with 
Abdulla's carpet to Shapeir.  Baba Yaga flew off towards the land of 
Mordavia.  The Hero hasn't seen the last of her.  With the hero's departure, 
someone needed to clean out the brigands, and Elsa took it upon herself.  She 
organized the guards and drove the band out of the valley.  For her actions, 
she was also given the title "Hero of Spielburg".

Barnard didn't approve of his little sister's hero status.  Their father 
retired, and he assumed the Barony.  Elsa, disgusted with her brother's means 
of running the valley, left Spielburg, and she took Toro the Minotaur with 
her.  They'll show up later in the series.

Also, Bruno's plans of grabbing the brigand's treasure were ruined with the 
hero's and Elsa's intervention.  The Thieves' Guild was cleaned out, but he 
managed to escape.  He's since gone to ground, and is plotting his revenge, 
not to mention looking for a new job.

******************************************************************************
9. MISCELLANY
******************************************************************************
=========
9A. FAQ =
=========

Q: How can I get this game?

A: It's tough to find nowadays.  Usually you'll find it only as a part 
of the Quest for Glory Anthology, which you can order from Sierra, or 
you could get lucky and find it at your local game store...

Q: How do I enter the workshop next to the tavern?

A: You can't.  The door's not even a sprite.

Q: I have a question about the VGA version of the game!  Can you help?

A: I have a separate FAQ for the VGA version on gamefaqs.com.

Just about all your other questions fit under the Walkthroughs...

================
9B. Point List =
================

All points are grouped in columns.  The first column is for the Fighter, 
the second for the Magic User, and the third for the Thief.

When the List says to "Question" someone, you need to ask them something 
they will respond to.  Nothing that they don't know the answer to.

The Town of Spielburg - 

Main Street:
1  1  1     Enter the town
1  1  1     Question the Sheriff

Hero's Tale Inn:
1  1  1     Question Shameen
5  5  5     Question Abdulla Doo at night
2  2  2     Give food to Abdulla Doo or order food for him
1  1  1     Eat food
1  1  1     Stay a night

Magic Shop:
1  1  1     Question Zara
   2        Buy the Open spell
   2        Buy the Fetch spell
   2        Buy the Flame Dart spell

Adventurer's Guild:
4  4  4     Read the Adventurer's Log
1  1  1     Sign name in Logbook
1  1  1     Question Guildmaster 
6  6  6     Read Quest Board

Little Old Lady's House:
      5     Break into House 
      1     Get silver from the drawers
      1     Get silver from the couch 
      1     Get silver from the purse 
      1     Get String of Pearls 
      1     Get Silver Candlesticks 
      3     Pet or feed the cat

Farmer's Mart:
1  1  1     Question Hilde 
3  3  3     Buy Apples

Dry Good Store:
1  1  1     Question Kaspar 
3           Buy Chainmail Armor

Sheriff's House:
      5     Break into Sheriff's House 
      1     Get silver from the desk 
      1     Get Music Box 
      1     Get Gold Candelabra 
      1     Get Alabaster Vase 
      1     Move painting 
      1     Open safe 
      1     Get silver from the safe

Alley:
1  1  1     Question Sam the beggar 
1  1  1     Give money to Sam 
      3     Make the Thief Sign to Slink at night

Tavern:
2  2  2     Get either secret note 
-5 -5 -5    Get drunk and be thrown out of tavern 
      5     Enter Thieves' Guild

Thieves' Guild: 
      3     Join Thieves' Guild (buy license)
      3     Buy Tool Kit 
      3     Fence stolen goods 
      3     Play Dag-Nab-It 
      5     Win 25+ silver in a single game of Dag-Nab-It 
  
Town Outskirts -

Main Road:
1  1  1     Leave town first time 
2  2  2     Question Bruno
10 10 10    Free the Fox

Archery Range:
12 12 12    Spy on Bruno and Brutus

Cemetery: 
2  2  2     Use Undead Unguent 
6  6  6     Get Mandrake Root

Healer's Hut:
3  3  3     Get Healer's Ring 
2  2  2     Question the Healer 
10 10 10    Return Healer's Ring 
1  1  1     Sell Magic Mushrooms 
1  1  1     Sell Magic Flowers 
2  2  2     Sell Cheetaur Claws 
2  2  2     Sell Troll Beard 
5  5  5     Give Magic Acorn 
2  2  2     Give Flying Water 
2  2  2     Give Green Fur 
2  2  2     Give Fairy Dust 
7  7  7     Get Dispel Potion

Spielburg Castle: 
5  5  5     Question Karl the Gate Keeper 
1  1  1     Enter the Castle 
1  1  1     Question Weapon Master 
3           Take fighting lessons 
10          Defeat Weapon Master 
5  5  5     Work in Stables 
10 10 10    Visit the Baron (after saving Barnard)
3  3  3     Question the Baron
25 25 25    Attend Hero Ceremony (after storming Brigand Fortress)

Farm: 
1  1  1     Question Heinrich 
3  3  3     Ask Heinrich about Brigand Leader 
  
Valley of Spielburg -

Forest:
1           Kill a Goblin 
1           Kill a Purple Saurus 
1           Kill a Brigand 
2           Kill a Mantray 
4           Kill a Saurus Rex 
4           Kill a Cheetaur 
4           Kill a Troll

Dryad Tree: 
1  1  1     Agree to help the Dryad
7  7  7     Give Seed to Dryad 
1  1  1     Get Magic Acorn 
-5 -5 -5    Eat the Magic Acorn 

Mushroom Ring:
3  3  3     Get Magic Mushrooms 
1  1  1     Question Fairies at night
3  3  3     Dance by choice at night
8  8  8     Get Fairy Dust at night

Meep's Peep:
1  1  1     Question Green Meep 
5  5  5     Get Green Fur  
   4        Learn Detect Magic spell

Baba Yaga's Hut:
2  2  2     Make deal with Bonehead 
10 10 10    Give Glowing Gem to Bonehead 
7  7  7     Make the hut sit down 
2  2  2     Visit Baba Yaga 
3  3  3     Give Baba Yaga the Mandrake root 
50 50 50    Turn Baba Yaga into a frog (after getting Magic Mirror)

Spore Spitting Spirea:
8  8  8     Get Spirea Seed

Erana's Peace: 
2  2  2     Eat magic fruit 
   5        Sleep in the meadow
   4        Learn Calm spell

Cave:
2           Kill Ogre 
5  5  5     Feed or Calm Bear 
2  2  2     Enter Kobold's cave area 
10 10       Kill Kobold 
7  7  7     Get the magic Key 
5  5  5     Get Kobold Treasure 
25 25 25    Free the Bear 

Mount Zauberburg:
3  3  3     Visit the Wizard Erasmus 
1  1  1     Question Erasmus 
   5        Play Mage's Maze 
  12        Win at Mage's Maze
 
Snow Field:
8  8  8     Get Glowing Gem from Brauggi 

Flying Falls Area:
1  1  1     Visit Spiegelsee 
3  3  3     Get Flying Water 
1  1  1     Knock on door 
5  5  5     Visit Henry the Hermit 
2  2  2     Question Henry 
   4        Learn Trigger spell 
  
Secret Passage to the Brigand Fortress:
10 10 10    Open secret passage 
5  5  5     Say the Password 
2  2  2     Enter secret passage

Brigand Fortress: 
5           Knock Out Minotaur 
8  8  8     Enter Brigand Fortress 
8  8  8     Enter Main Hall 
8  8  8     Enter Warlock's Room 
2  2  2     Question Brigand Warlock 
8  8  8     Ask Warlock about Elsa or Yorick
12 12 12    Enter Leader's Room 
35 35 35    Throw Dispel Potion on Brigand Leader
10 10 10    Get Magic Mirror

==============================
9C. Frequently Missed Points =
==============================

Here are a bunch of deeds that people often miss, or seem to be easily 
missed.  Also, this section details parts of the game that you must do 
properly to get full credit.

Questions, Questions -
 Make sure you interrogate everyone that can be in the game.  To get all 
 the points, you need to "ask about" anything to the Sheriff, Shameen, 
 Abdulla Doo, Zara, Wolfgang, Kaspar, Hilde, Sam, Bruno, the Healer, Karl 
 the Gatekeeper, the Weapon Master, the Baron, Heinrich, the Fairies, the 
 Green Meep, Henry, and Erasmus.

Generosity's Reward -
 It doesn't seem like an obvious thing to do, but make sure you order food 
 for Abdulla at night when Shema's near the two of you.

Professional Purloiner -
 Things to do in LOL's house: Get silver from the drawers, the couch, the 
  purse, take the candlesticks, the pearls from the sewing basket, and feed 
  or pet the cat.
 Things to do in Sheriff's house: Take the silver in the desk and safe, and 
  take the candelabra, vase, and music box.

Hidden in Plain Sight -
 Back in the day, I never knew what that white ball near the stool was in 
 the tavern.  Make sure you take that secret note (either one's fine).

Taught a Lesson -
 Build up your skills, Fighter, because you have to take down the Weapon 
 Master.  Repeatedly attack until you push him against the back wall and 
 he'll give up.

https://gamefaqs.gamespot.com/pc/564775-quest-for-glory-i-so-you-want-to-be-a-hero/faqs/1988
START OF THE GAME

    When you start playing, you 'construct' a character according to your
    wishes. You may choose from a fighter, a magician or a thief. Probably,
    the game can be solved using any character. I chose for a fighter; so the
    remainder of this text discusses mainly warrior-like aspects.

    After you choose a character, you get 50 points to divide between certain
    aspects of your character. A warrior does OK when fighting but poorly in
    magic; a thief has a good stealth and can pick locks, etc.. Make sure that
    all categories are non-zero! You acquire skills during the game; but the
    startup skill may not be zero to accomplish this. So, even when you are a
    warrior, make sure that your magic category counts some points. I left the
    'stealth' category at zero; it worked out OK.


SO YOU WANT TO BE A HERO -- GENERAL HINTS

    While playing, take all action you can! Your skills will improve by doing
    so. E.g., fight monsters as often as you can; use your magic skills
    whenever possible; etc..

    Make a map of the area. Simply start playing by wandering around and
    looking at stuff. The map of the area is not included in this text; but
    you should be able to figure it out easily.

    Save your game often! You are likely to get killed, to fall into pits,
    etc..

    Generally, there are two things you should check regularly: your
    character's sheet (press ctrl-S to see it) and the time of day (press
    ctrl-T). Watch your sheet for bad health (lower left corner of the
    display). When you are too weak, you make mistakes. Furthermore, there is
    a good chance that you will not survive an attack by monsters.

    The time of day is equally important. There are more monsters in the woods
    during night time. Furthermore, it is wise to get some sleep during night
    hours --- your strength, stamina, etc. will be restored by it. Then there
    are tasks which may be accomplished only during a certain time of day.


PHASE ONE: ACQUIRING SKILLS

    When you start in the town of Spielburg, talk to the sheriff and ask about
    stuff. Check out the city; be sure to visit the Guild Hall (where the
    proprietor is asleep all of the time) to look at the board. There are some
    'wanted' notes hanging here: remember the names (you'll need them!).
    There's a magic shop in town; you can buy scrolls there (to learn spells)
    if your magic skill is high enough. If not, you can buy magic potions;
    drinking these will improve your character's magic abilities until you CAN
    buy scrolls. Pay attention to what the witch in the magic shop says about
    spells! A 'free' spell can be learned in the woods.

    The potions you can buy will help you on your way. There are health
    potions (drink them when your health has suffered from battle), vigor
    potions (which you may use when getting tired), magic potions (to be used
    when your magic power gets too low due to excessive spell casting), etc..
    However, potions can be acquired at a lower price at the healer's; out of
    town and then N. The only advantage of the magic shop in town is that it
    offers 24 hours service.

    Don't buy food or drink at the local tavern! You will need your funds for
    more important matters.

    If sunset is not yet approaching, leave town and head E by following the
    road. At the end of the road go N. In an snowy clearing you will meet a
    warrior giant: bargain with him. He will want fruit from you: get it in
    town (in fact, get LOTS of it: the warrior is pretty big. You'll need
    about 50 apples). The giant will give you something you will need later on
    for your trouble.

    If you still have the time, return to the town entrance and go N. You'll
    find the healer's cottage there. Notice the bird in the nest on the branch
    of the large oak? Try to climb the tree to get to the nest! That may take
    some time, if your climbing is not yet too good. Be sure to watch your
    health during the work-out! Rest a couple of times.

    If you make it up the tree, look in the nest and you'll see a ring.
    Remeber the 'wanted' signs at the Guild Hall? Try to get the ring: you may
    fall off the tree if your climbing is not good enough yet. Watch your
    health! Falling off trees is not good for you.

    If sunset approaches, go back into town --- even without the ring. If you
    do get the ring, return it to the healer first (you'll get money and a big
    kiss). In town, wait for dark; then go N of the town entrance to the
    low-life's pub. There is an alley next to the bar; a beggar sits there
    during daytime (don't give him money; he'll just advise you not to take up
    begging since it doesn't pay). During the night, two thugs hide in the
    alley; ready to rob you. When they make their move, make the thieve's
    sign! They will take you for one of their own; and give you the password
    to get into the Thives Guild.

    Go to the bar. Notice the ugly looking guy sitting in a chair at the left
    side of the bar? Go up to him, and give him the password. He'll let you
    into the Thieves Guild.

    Once in the Thieves Guild, you'll encounter the boss. He rambles on about
    the times being so hard; and then turns his attention to you. You'll have
    to get a Thieve's Licence at the far end of the room before continuing
    your talk with the boss. Once you are a registered thief, ask the boss
    about that game he's playing, and play it with him a couple of times.
    First you'll win some money; but when you start betting too much, the boss
    decides to teach you a lesson --- time to stop playing. Before you leave,
    be sure to buy a lockpick at the far end of the room where you got your
    licence. Then leave the Thieves Guild.

    During night time, you can pick a couple of locks in town and steal stuff.
    There are some silvers in the desk in the Sheriff's house (that's not the
    office where you see him during daytime) and a precious vase on the
    mantlepiece. Don't wake anybody up! In the other street, next to the magic
    shop, you can enter the old lady's house to get the candles and to get the
    basket (the poor old thing dropped her pearls in the knitting basket ---
    good for you). Don't approach the cat! When you get too close, it turns
    into a ferocious monster. You can sell the stolen stuff in the thieve's
    guild.

    It should be day again by now. Go to the grocery store, and talk to the
    proprietor. He will tell you that he actually wanted to be a hero himself,
    and that he has some useful materials. Buy a dagger if you don't have one
    yet; but don't buy empty flasks (even if the manager tells you that
    they're o-so-handy). Also, he'll tell you about the chain mail armor you
    can buy: but you need lots of money for that. You will have to get the
    money anyway, eventually!

    Before leaving town, check out the magic shop again. Can you buy spells
    yet? If so, get some of the spells. If not, you will have to improve your
    magic first and come back later.


PHASE TWO: GETTING THE MAGIC STUFF

    If you hadn't retrieved the ring yet, do so and bring it to the healer.
    Buy some potion (especially health potion), and ask the healer about some
    things. E.g., ask about Elsa, the lost baron's daughter and about the
    creatures you have seen during your trip around the perimeter. Then, go
    see the Dryad. That creature lives roughly SW of the town (a white deer
    will show you the way, if you get close enough to the right place). While
    underway, again fight as many monsters as you can. Be sure to search the
    dead body after the fight: the humanoid monsters usually carry cash.

    The Dryad lives in a magic acorn. Approach the tree, and the Dryad will
    appear. Talk to her (him? it?), and listen closely. The Dryad will ask you
    to get seeds from plants which grow roughly N from the Dryad's place. Go
    there, get the seed (you'll have to climb a rock and try to catch the
    seed) and go back to the Dryad. Here, you will get information about a
    magic ointment you will certainly need. The first ingredient (an acorn)
    can be found under the tree when the Dryad disappears.

    As for the other ingredients: Magic flowers can be found at Erana's Peace:
    a nice meadow with a fruit tree. This place lies roughly NE from the town.
    You can spend your nights here, and eat the fruit. Your staying at this
    place is always refreshing. Read the runes on the stone, cast an open
    spell to move it, reach in the hole and get the scroll which is kept
    there. Read the scroll: you'll learn a new spell. Flying water can be
    found at the waterfall. There is a ledge with a door here; climb the rock
    (there is only one place to do it) and knock on the door. Be sure to step
    aside quickly: the door swings open wide. Enter the cave, and talk to
    Henry the Hermit. Share your rations with him; and ask him about magic
    stuff. He'll give you a scroll for your kindness: read the scroll, and
    learn a new spell. Henry will offer you to spend the night: you may do so,
    but it will not bring you advantages (the guy talks all the time, so you
    don't sleep too good). Green fur can be found at the Meeps' place; be kind
    to the little guys. Talk to them (although they can't provide much
    information). Eventually, ask for the fur and take it with you. The last
    ingredient, Fairy Dust, can be found at the ring of mushrooms NE from the
    Dryad's place. However, you must be there at night otherwise the fairies
    will not appear. Dance for the fairies to win their sympathy. After you
    get the dust, take some mushrooms along to sell to the healer. Spend the
    rest of the night at Erana's Peace, sleeping peacefully.

    The next day go to the healer and give her the ingredients. She will make
    a dispel potion, which you'll need later on. You'll have to leave the
    healer alone though, while she prepares the potion. During that period try
    to increase your fighting and magic skills. Here's one approach: Get the
    Flame Darts spell from the magic shop, and then just wonder around. When
    you see an enemy, cast the Flame Darts spell a couple of times, and then
    engage in battle. If you win, both your fighting and your magic skills
    will have improved. Always be sure to carry enough healing potion though!
    If you run out, and are too weak, you may get killed in battle. Also
    remember to rest after a battle, to get your stamina points up.

    The goblins, living W from the town, are pretty good battle opponents. You
    fight only as much goblins as you can handle, and they usually carry
    enough cash to enable you to buy a fresh healing potion.

    When you feel strong enough, get the dispel potion from the healer (it
    should be ready by now) and go to Baba Yaga, who lives NW from town. Talk
    to the skull in the middle of the fence: this guy will want something you
    got at the beginning of the game. As a reward, he will let you pass.
    Approach the hut, and say: 'Hut of brown now sit down' (you can get that
    rhyme in various places, also from the skull). Then enter the hut. Baba
    Yaga is a pretty mean witch: she'll change you into a frog to eat you.
    But, don't worry: she'll change you back again, because she has an
    assignment for you: you have to bring her some mandrake root, growing in
    the town graveyard. However, you have to pull the root exactly at
    midnight. So here comes the spooky part: you have to wait until the middle
    of the night (waiting by the target at the S town wall is a good place
    because monsters stay away) and then you have to get to the graveyard.
    There are some zombies here who will want to suck your life out of you.
    Best is to approach the graveyard from the N side, to get the root, and to
    leave as fast as possible towards S. You'll get killed a couple of times
    until you succeed though. When you have the root, bring it to Baba Yaga
    again. She'll let you live as a reward for your services.


PHASE THREE: HOW'S YOUR FIGHTING SKILL?

    Here comes the 'boring' part: you have to improve your skills greatly
    before you can accomplish your hero's mission. If you are a warrior, fight
    during the day to get money and skills, and sleep at Erana's Peace during
    the night to restore your body. If you are a magician or a thief, do
    whatever these guys are supposed to do.

    You have to get enough money to buy the armor, sold at the grocer's in
    the town. When you have the armor, make sure that you have two vigor
    potions for your stamina and two health potions for your wounds; and then
    go to the brigand cave which lies SE from the town. On your way there, you
    will pass an antwerp (whatever beast that may be). Don't make this guy
    angry: he can get mean! When you reach the brigand's cave, enter it.


THE LAST PART OF YOUR MISSION

    Lots of archers appear at the sides of the cave; but the hero, not knowing
    fear, must go on with the mission. Walk through the cave, avoiding as many
    arrows as you can. When you reach the logs at the W side of the cave,
    drink a health potion and a vigor potion to prepare for battle; then climb
    the logs. Here you will encounter three brigands which you have to slay.

    Moving on, you encounter a Minotaur. Again drink health potion and vigor
    potion before you engage in battle. If you have the time, fire some flame
    darts at the sucker to hurt him (else, he'll hurt you the more).

    If you don't survive the battle, then your fighting is still not good
    enough; and you'll have to go on practicing and trying. But if you do
    survive, you can get into the brigand's fortress by opening the gate (use
    your magic). Go through the court to the far side (N side of the court),
    but choose your way carefully: you may fall over trip wires, fall into
    traps, etc.. Once you get there, open the door on the NE side of the court
    and get in.

    Here comes the hairy part: you have to move real quickly here. Close and
    bolt the door behind you (otherwise, you get surrounded and killed). Then
    there are three more doors you have to deal with: one in the NE corner of
    the room (move the chair, so that it gets barred), and two more in the NW
    corner. While working with the chair, some bad guys will enter from one of
    the remaining doors. Push the candelabra to bar their way, then climb the
    table and get the rope to do some Errol Flynn stuff. Quickly exit through
    the N door, before more bad guys come in.

    Now you are in the anteroom of the Brigand Leader's office. The leader's
    guard is sitting here in a room which looks like a toy shop. However,
    there are deadly traps here! Talk to the guy about Elsa (you should know
    who that is, having read the board in the Guild Hall). He'll become quite
    friendly (when he's not friendly, he's not so good to be around with).
    Then start walking; the guy will stand up and say he'll meet you in a
    minute in the Leader's office. However, you still have to get through the
    toy room. Walk right until you get to a trap, then walking diagonally step
    over the trap. Take the first door on your right. You'll pass a maze and
    end up at the left side of the room, looking at a chain. Pull the chain,
    and now be very quick: go left through the maze again to get to the right
    hand door, then move to the door which the chain opened. If you make it in
    time, you'll end up at the left hand side of the room again, but near a
    different door. Open the door, then quickly step aside (the door will
    otherwise fall on your head), and go through the opening.

    You are in the Brigand Leader's office now, facing the Big Bad Guy. But
    wait a minute, doesn't that look in the leader's eyes remind you of
    something? Now is the time to use the dispel potion; and before your eyes
    the brigand leader will change into Elsa, the lost baron's child. (She
    rather looks like a country maid; personally, I liked the brigand leader
    better.) Yorick, the leader's guard, now comes in looking quite happy.
    Watch where he came from! That's the secret way out! Elsa now will tell
    you that she's leaving with Yorick, but she'll offer you a mirror (you
    should know by now what it is useful for) and two healing potions. Take
    both, and leave before the brigands come in.


THE END OF THE GAME

    Your quest is nearly finished now. There is one more bad creature you have
    to deal with: the Big Bad Baba. Go see her, and when she tries to turn you
    into a frog again, deal with her using a valuable magic object you now
    have.

    Then you can go to the castle, where everyone's waiting for you: the Hero.


SUGGESTIONS

    The course of the game described above doesn't get you the maximum points.
    E.g., there is the Wizard's Castle, where you can get in to talk to the
    wizard (you can get valuable information there). When your magic is good,
    you can challenge him for a game of spells casting: if you succeed, he
    teaches you a new spell. Also, there is a bear cave with a big ugly bear
    chained to the cave wall. The lock of the chain cannot be opened using a
    picklock: it is a magic lock. I don't know what happens when you free the
    bear. One more suggestion is to practice throwing your dagger at the
    target. Some local thugs come here too; if you hide in the bushes, you can
    overhear them exchanging valuable information. And then there is the
    'undead unguent': quite expensive stuff you buy at the healer's. It should
    protect you from the zombies at the graveyard during the night; but it
    didn't work in my case (so I must have done something wrong).

https://www.sierrachest.com/index.php?a=games&id=15&title=quest-for-glory-1&fld=walkthrough&pid=100
Fighter

Location
Action
Points


INITIAL STEPS


Town entrance
You enter town
1/1


Ask the sheriff about brigands, magic, Otto, merchant, heros and monsters.
1/2
Hero's Tale Inn
Ask Shameen about the merchant (you learn his name is Abdulla and that he'll be there around suppertime), robbery, wealth, kattas and food.
1/3


Sit down (Shema enters) and order food. Eat the food.
1/4


Stand, exit, go west and enter the magic shop.


Magic shop
Ask about town, valley, aura, Erasmus, Erana, Baba Yaga, the curse, magic, potions, and spells.
1/5


Exit and enter the guild hall to the left.


Guild hall
Read the Adventurer's logbook (4) and sign it (1).
5/10


Look at the board.
6/16


Read the individual quests. There are 6 of them:
- Reward for return of lost ring. Inquire at the Healer's.
- Reward of 50 gold coins for the safe return of Elsa von Spielburg. Inquire at Spielburg Castle gates.
- Reward of 30 gold coins for the capture or death of the Brigand Warlock. Description: short, ugly, and wears brightly colored robes. Has habit of laughing continually. Inquire at Spielburg Castle gates.
- Wanted: Brigand Leader. Description: Unknown appearance, wears a cloak. Must provide proof of leader's identity. Reward of 60 gold coins and title of Hero of the Realm. Inquire at Spielburg Castle gates.
- Notice: Spell components needed. Cash or trade for potions. Inquire at the Healer's.
- Reward of 50 gold coins for information leading to the return of Baronet Barnard von Spielburg. Inquire at Spielburg Castle gates.




Wake up the Guildmaster, Wolfgang, and question him about Barnard, Elsa, Yorick, monsters, brigands, Baba Yaga and the curse.
1/17


Leave, go past the sheriff's office and north to Market Street.


Dry goods store
Ask Kaspar about equipment and armor.
1/18


Exit, go west to Tavern Street and enter the alley.


Alley
Give Sam the beggar one silver.
1/19


Ask Sam about the night and brigands.
1/20
Tavern
Get the note from underneath the stool.
2/22
Market
Ask Hilde the centaur about Brigands, fruit, vegetables, father.
1/23


Leave and wander in town (don't leave town yet!) until sunset. Then enter the inn.


Hero's Tale Inn
Sit and ask Abdulla about brigands, the brigand leader, minotaur, his name, and Shapeir.
5/28


Give Abdulla a ration. He'll tell you about his magic carpet, which was among the stolen items.
2/30


Go to Shameen and pay room`(5 silvers).
1/31


Leave town (1), and go all the way east to the avalanche. Then travel west and back east until you see a trapped fox.
1/32
Trapped fox
Free fox and he'll give information on how to break Baba Yaga's curse.
10/42


Head back outside the town entrance and one north to the healer. Knock on the door and enter.


Healer
Ask about potions, components, and her ring.
2/44


Go outside, get rocks and throw rocks (at the nest) until it falls and you recover the ring. (when you run out of rocks, get rocks again).
3/47


Return inside and give the ring to the healer. You'll get six golds and two Healing Potions
10/57


Exit and go one screen west to the farm.


Farm
Ask Heinrich about the centaur about his name, his daughter Hilde, the farm, and the brigands (1). Also ask him about the leader (3).
4/61


Go east and north to the castle gate.


Castle gate
Ask Karl the Gatekeeper about the castle, the baron, Barnard, Elsa, Baba Yaga, and the curse.
5/66


Open gate and enter the courtyard.
1/67
Courtyard
If the weapon Master is there, ask him about fighting.
1/68


Answer yes twice and you'll pay a gold coin to train with him (side view battle). Since your stats are still low, you'll lose.
3/71


Rest a few times to increase stamina




Go right to the stables.


Stables
Enter the stables and type "yes" when asked to work. You'll get 5 silver coins.
5/76



BUILDING UP THE STATS


Easy combat
From here on you'll run into monsters. At first make sure to wander around during the day (during the night, you'll run into deadlier foes until you reach 1000 experience, after which they also appear during the day) and kill random creatures (save regularly). Rest after each fight (repeat until the hero refuses to rest more). You'll run into three different foes: Goblins, Purple Saurus and Brigands. Goblins are the easiest. You may also want to try fighting a Purple Saurus. Avoid the Brigand until your stats are high enough (30 or above). Search the bodies of goblins and brigands each time for loot (Purple Saurus doesn't have any). As a warrior, you'll earn a point for the first of each easy monster type you kill. When health gets low, have a healing potion (you'll get an empty flask after use) or avoid combat until the next day after you've recovered at Erana's Peace. You can buy additional healing potions at the healer for 40 silvers a piece.
3/79
Throwing skill
Enhance your throwing skill to 100 by going in the woods, getting rocks from the ground and throwing them away (no target needed).


Castle
Practice each day with the weapons master in the courtyard for a stats boost. Feel free to clean the stables daily also for 5 silvers.


Goblin camp
From the farm go 3 west to find a group of goblins. These are excellent for combat practice and money. You start fighting 1 goblin and each time you fight there again, you'll fight an extra goblin. Search all bodies to earn 56 silvers each time.


Erana's Peace
At the end of each day, you can eat and sleep (and consequently recover health and stamina) at Erana's Peace. To get there, go from the farm, four screens north, one east, and one north. Run to avoid monsters.




Eat fruit from the tree each day to avoid using rations (2 points the first time). Sleep.
2/81
Hard combat
When experience exceeds 1000, tougher opponents will appear. The Cheetaur (4) and Mantray (2), which previously only appeared during the night, now also appear during the day. Make sure to get the Cheetaur's claws after killing it. A new monster is the Saurus Rex (4), which appears both day and night. During nighttime only, the troll appears (4). Search its body and get its beard.
14/95
Castle
After maxing out most of your stats, you manage to beat the weapons master in the castle (10). Make sure stamina is high before attempting.
10/105
Dry goods store
If you have 500 silver (1 gold is 10 silver), buy armor.
3/108



DISPEL POTION


Erana's Peace
After a good night sleep at Erana's Peace, pick flowers.


Town entrance
From the town entrance, go 2 south and 3 west, and follow the stag. Don't hurt it!


Dryad
Approach the tree and the Dryad will appear, asking if you're a friend of the forest. Answer yes and the Dryad will ask to bring a seed from the Spore Spitting Spirea in the north.
1/109


Go 1 east and one north.


Mushroom circle
Get mushrooms. (feel free to eat them, but save first).
3/112


Go 1 east, 4 north, 1 east and 1 north.


Spore Spitting Spirea
As a warrior, throw rock at seed, and you'll pick it up.
8/120
Dryad
Answer yes and you'll give the seed (7). The Dryad will tell you of a dispel potion the healer can make for you. It requires flowers from Erana's Peace, Green fur, Fairy Dust, a Magic Acorn, and flying water.
7/127


Get the acorn that falls from the tree.
1/128


Go 1 east, 2 north and 2 west.


Meeps
Talk to meeps and ask about meeps, fur and green fur.
1/129


Get the green fur
5/134


If you have magic skill, then also the "detect magic" spell scroll can be picked up.
0/134


Go 2 east, 2 south, and 5 east


Hermit
Get water (requires empty flask)
3/137


Throw 3 rocks at the door. The hermit will come out and show you the ladder.




Climb the ladder, stand right off the door and knock to get in.
6/143


Ask about hermits, ladder, cave, magic, brigands, spell, scroll (if you have magic skill).
2/145


If you have magic skill, ask about spell and scroll, and get the scroll of the trigger spell.
0/145


Leave the hermits house and climb down the ladder. Then go 1 west and 1 south.


Mirror Lake
Just for entering the screen, you get 1 point.
1/146


Exit and re-enter for a Code Name: Iceman Easter egg.


Town entrance
If Bruno is there (he appears from midday), try asking him something. Give him a silver, and another silver. Ask about Baba Yaga about the Castle, and goblins. Don't give money for more info. You get 2 points when you leave the screen.
2/148
Healer
Give acorn
5/153


Give green fur
2/155


Give flowers. You'll get 5 silvers.
1/156


Give flying water.
2/158


Give mushrooms. You'll get one gold.
1/159


Give Sheetaur claws for 5 silver each.
2/161


Give troll beard. You get 2 healing potions for each.
2/163
Mushroom ring
Make sure you have an empty flask. At night the fairies will be there. Answer yes when they ask you to dance.
3/166


Ask about fairies, dryad and fairy dust, and you'll get some.
9/175
Healer
Give fairy dust
2/177


Exit and re-enter and you'll get the dispel potion
7/184



ERASMUS


Healer
From the healer, go east, north, and twice east. You'll see Erasmus' house in the distance. Read the boards and go up the mountain.


Erasmus outside
The Gargoyle will ask three questions. Possible questions are:
- Name: Your name as you entered it
- Quest: "Glory"
- Favorite color: "Purple"
- Whose spell protects the town: "Erana"
- Who do you seek: "Erasmus" or "Fenrus"
- Baron's first name: "Stefan"
- Thieves' Password: "I don't know"
Then enter the house.
3/187
Erasmus house
In the hallway look at the objects for some other Sierra game references, such as Police Quest, King's Quest 4, The Colonel's Bequest and Conquests of Camelot. Don't touch anything though and proceed up the stairs.
In Erasmus tower room, ask him about Erana, Baba Yaga, Zara, Warlock, Henry. Also, ask him about curse and countercurse.
Stand up and he'll send you to the bottom of the mountain.
1/188



BABA YAGA


Healer
Buy a potion of undead unguent for 100 silver


Market
Buy 50 apples from Hilde (5 silvers). This will overload the hero, considerably slowing him down.
3/191


Leave town, go 2 screens east, go north and east.


Brauggi
Say bargain and give the fruit. You'll get a glowing gem.
8/199


From the farm, go three screens west and two screens north.


Baba Yaga
Ask Bonehead (the gate skull) about eyes, deal and say "yes"
2/201


Ask Bonehead about the hut and the rhyme




Give the glowing gem and the gate will lower.
10/211


Type "hut of brown now sit down" (7) and enter (2).
9/220


Tell her your name, agree to help, and agree to go to the graveyard and get some Mandrake.


Woods
Wait until midnight (when it says "You are getting tired"), use the Undead Unguent (2) and head to the graveyard (left of town).
2/222
Graveyard
Get the little red plant (6) and return immediately to Baba Yaga.
6/228
Baba Yaga
Say yes and you'll give the mandrake to her
3/231



FREE BARNARD VON SPIELBURG


Ogre
The ogre, a unique enemy, is 1 south and 1 east from Erana's Peace. Fight and kill it.
2/233


Break or force the chest to open it. You'll get 1 gold and 43 silver.




Enter the cave.


Cave
Feed the bear with an apple vegetable or ration.
5/238


Talk twice to the bear for a Talking Bear Easter Egg




Walk past the bear into the next cave (2) and fight the kobold (10).
12/250








Get the key
7/257


Walk at the very bottom of the cave until you bump into something invisible. Force chest (you'll lose a bit of health) and get gold (10 gold and 60 silver).
5/262


Return to the bear and free bear
25/287
Castle
Enter the castle
10/297


Ask about brigands, brigand leader, warlock, prophecy, Elsa.
3/300



FREE ELSA VON SPIELBURG


Tavern
Get the note from underneath the stool (2 points if you didn't pick up the first note).
0/300


At noon, head for the archery south of town (from the town exit, 1 south and 1 west).


Archery
Witness Bruno's and Brutus' meeting. Remember the password: "Hiden Goseke".
12/312


Enter the archery along the south side (quick so Bruno doesn't spot you).




Kill Brutus and get the key.




Go 1 south, 2 west and 1 south to the Antwerp (the "bouncer").


Antwerp
Go around the Antwerp, unlock the door with the key and open it.
10/322


Type "Hiden Goseke". Fred the Troll will go to the back of the cave.
5/327


Enter the cave.
2/329
Cave
Take the left tunnel to find Fred's treasure. Fight Fred, get his beard and search the pile (5 gold and 30 silver)




Return right and take the lower right exit.


Fortress Gate
Defeat Toro, the Minotaur (5) and search him.
5/334


Force gate and enter the fortress.
8/342
Fortress courtyard
Go to the left, cross the right bridge, "step over rope" and go through the back right door.
8/350
Mess hall
SAVE game! Close door. Go to the upper right door and, as soon as the brigands start to leave, "move chair". Go to the candelabra and, as the first of three brigands walks behind the table, "move candelabra". Quickly walk in front of the table and climb table. Walk through the back left door.
8/358
Brigand Warlock
Ask about Elsa (2) or Yorick (8)
10/368


Walk through the first door on the left, then the first on the right. (If you start rolling, type "stop".) Pull the chain to open the back right door. Head back and go through it. Open the painted door, quickly move aside and, after it dropped down, enter it.
12/380
Brigand leader
Throw the Dispel Potion. It's Elsa Von Spielburg!
35/415


After Elsa disappears, go to the desk, get healing potions and get mirror.
10/425


Leave through the right side





BABA YAGA


Antwerp
Go 1 north, 1 west and 5 north


Baba Yaga
Bone head lets you in. Say "Hut of Brown, now sit down" and enter.




As you enter, immediately hold mirror and Baba Yaga will turn herself into a frog.
50/475
Castle
Reward ceremony
25/500


THE END
500/500


Magic user

Location
Action
Points


INITIAL STEPS


Town entrance
You enter town
1/1


Ask the sheriff about brigands, magic, Otto, merchant, heros and monsters.
1/2
Hero's Tale Inn
Ask Shameen about the merchant (you learn his name is Abdulla and that he'll be there around suppertime), robbery, wealth, kattas and food.
1/3


Sit down (Shema enters) and order food. Eat the food.
1/4


Stand, exit, go west and enter the magic shop.


Magic shop
Ask about town, valley, aura, Erasmus, Erana, Baba Yaga, the curse, magic, potions, and spells.
1/5


Buy the fetch spell for 40 silvers (4 gold).
2/7


Exit and enter the guild hall to the left.


Guild hall
Read the Adventurer's logbook (4) and sign it (1).
5/12


Look at the board.
6/18


Read the individual quests. There are 6 of them:
- Reward for return of lost ring. Inquire at the Healer's.
- Reward of 50 gold coins for the safe return of Elsa von Spielburg. Inquire at Spielburg Castle gates.
- Reward of 30 gold coins for the capture or death of the Brigand Warlock. Description: short, ugly, and wears brightly colored robes. Has habit of laughing continually. Inquire at Spielburg Castle gates.
- Wanted: Brigand Leader. Description: Unknown appearance, wears a cloak. Must provide proof of leader's identity. Reward of 60 gold coins and title of Hero of the Realm. Inquire at Spielburg Castle gates.
- Notice: Spell components needed. Cash or trade for potions. Inquire at the Healer's.
- Reward of 50 gold coins for information leading to the return of Baronet Barnard von Spielburg. Inquire at Spielburg Castle gates.




Wake up the Guildmaster, Wolfgang, and question him about Barnard, Elsa, Yorick, monsters, brigands, Baba Yaga and the curse.
1/19


Leave, go past the sheriff's office and north to Market Street.


Dry goods store
Ask Kaspar about equipment and armor.
1/20


Exit, go west to Tavern Street and enter the alley.


Alley
Give Sam the beggar one silver.
1/21


Ask Sam about the night and brigands.
1/22
Tavern
Get the note from underneath the stool.
2/24
Market
Ask Hilde the centaur about Brigands, fruit, vegetables, father.
1/25


Leave and wander in town (don't leave town yet!) until sunset. Then enter the inn.


Hero's Tale Inn
Sit and ask Abdulla about brigands, the brigand leader, minotaur, his name, and Shapeir.
5/30


Give Abdulla a ration. He'll tell you about his magic carpet, which was among the stolen items.
2/32


Go to Shameen and pay room`(5 silvers).
1/33


Leave town (1), and go all the way east to the avalanche. Then travel west and back east until you see a trapped fox.
1/34
Trapped fox
Free fox and he'll give information on how to break Baba Yaga's curse.
10/44


Head back outside the town entrance and one north to the healer. Knock on the door and enter.


Healer
Ask about potions, components, and her ring.
2/46


Go outside and cast fetch to recover the ring from the nest.
3/49


Return inside and give the ring to the healer. You'll get six golds and two Healing Potions
10/59
Magic shop
Buy the open spell for 30 silvers.
2/61


Exit and go one screen west to the farm.


Farm
Ask Heinrich about the centaur about his name, his daughter Hilde, the farm, and the brigands (1). Also ask him about the leader (3).
4/65


Go east and north to the castle gate.


Castle gate
Ask Karl the Gatekeeper about the castle, the baron, Barnard, Elsa, Baba Yaga, and the curse.
5/70


Open gate and enter the courtyard.
1/71
Courtyard
If the weapon Master is there, ask him about fighting.
1/72


Unlike the fighter, the magic user can't train with him.




Go right to the stables.


Stables
Enter the stables and type "yes" when asked to work. You'll get 5 silver coins.
5/77



BUILDING UP THE STATS


Easy combat
From here on you'll run into monsters. As a magic user, you can cast zap to charge your weapon with magical energy. You can also cast calm or flame dart. At first make sure to wander around during the day (during the night, you'll run into deadlier foes until you reach 1000 experience, after which they also appear during the day) and kill random creatures (save regularly). Rest after each fight (repeat until the hero refuses to rest more). You'll run into three different foes: Goblins, Purple Saurus and Brigands. Goblins are the easiest. You may also want to try fighting a Purple Saurus. Avoid the Brigand until your stats are high enough (30 or above). Search the bodies of goblins and brigands each time for loot (Purple Saurus doesn't have any). As a magic user, you won't earn points for the first of each easy monster type you kill. When health gets low, have a healing potion (you'll get an empty flask after use) or avoid combat until the next day after you've recovered at Erana's Peace. You can buy additional potions at the healer.


Spells
When you're low on health, cast spells without opponent to further enhance spell points.




When you have 60 silvers, go to the magic shop to buy the Flame Dart spell.
2/79
Castle
Feel free to clean the stables daily for 5 silvers.


Goblin camp
From the farm go 3 west to find a group of goblins. These are excellent for combat practice and money. You start fighting 1 goblin and each time you fight there again, you'll fight an extra goblin. Search all bodies to earn 56 silvers each time.


Erana's Peace
At the end of each day, you can eat and sleep (and consequently recover health and stamina) at Erana's Peace (5 points the first time). To get there, go from the farm, four screens north, one east, and one north. Run to avoid monsters.
5/84


Look runes and cast open to get the Calm Spell.
4/88


Eat fruit from the tree each day to avoid using rations (2 points the first time).
2/90
Hard combat
When experience exceeds 1000, tougher opponents will appear. The Cheetaur and Mantray, which previously only appeared during the night, now also appear during the day. Make sure to get the Cheetaur's claws after killing it. A new monster is the Saurus Rex, which appears both day and night. During nighttime only, the troll appears. Search its body and get its beard.


Dry goods store
If you have 500 silver (1 gold is 10 silver), buy armor.





DISPEL POTION


Erana's Peace
After a good night sleep at Erana's Peace, pick flowers.


Town entrance
From the town entrance, go 2 south and 3 west, and follow the stag. Don't hurt it!


Dryad
Approach the tree and the Dryad will appear, asking if you're a friend of the forest. Answer yes and the Dryad will ask to bring a seed from the Spore Spitting Spirea in the north.
1/91


Go 1 east and one north.


Mushroom circle
Get mushrooms. (feel free to eat them, but save first).
3/94


Go 1 east, 4 north, 1 east and 1 north.


Spore Spitting Spirea
As a magic user, cast fetch to get the seed.
8/102
Dryad
Answer yes and you'll give the seed (7). The Dryad will tell you of a dispel potion the healer can make for you. It requires flowers from Erana's Peace, Green fur, Fairy Dust, a Magic Acorn, and flying water.
7/109


Get the acorn that falls from the tree.
1/110


Go 1 east, 2 north and 2 west.


Meeps
Talk to meeps and ask about meeps, fur and green fur.
1/111


Get the green fur
5/116


Get the "detect magic" spell scroll.
4/120


Go 2 east, 2 south, and 5 east


Hermit
Get water (requires empty flask)
3/123


Cast detect to reveal the hidden ladder and climb it.




Climb the ladder, stand right off the door and knock to get in.
6/129


Ask about hermits, ladder, cave, brigands, spell.
2/131


Ask about scroll, answer yes and get the scroll to obtain the trigger spell.
4/135


Leave the hermits house and climb down the ladder. Then go 1 west and 1 south.


Mirror Lake
Just for entering the screen, you get 1 point.
1/136


Exit and re-enter for a Code Name: Iceman Easter egg.


Town entrance
If Bruno is there (he appears from midday), try asking him something. Give him a silver, and another silver. Ask about Baba Yaga about the Castle, and goblins. Don't give money for more info. You get 2 points when you leave the screen.
2/138
Healer
Give acorn
5/143


Give green fur
2/145


Give flowers. You'll get 5 silvers.
1/146


Give flying water.
2/148


Give mushrooms. You'll get one gold.
1/149


Give Sheetaur claws for 5 silver each.
2/151


Give troll beard. You get 2 healing potions for each.
2/153
Mushroom ring
Make sure you have an empty flask. At night the fairies will be there. Answer yes when they ask you to dance.
3/156


Ask about fairies, dryad and fairy dust, and you'll get some.
9/165
Healer
Give fairy dust
2/167


Exit and re-enter and you'll get the dispel potion
7/174


Purchase two or three magic potions if you don't have them.





ERASMUS


Healer
From the healer, go east, twice north, and twice east. You'll see Erasmus' house in the distance. Read the boards and go up the mountain.


Erasmus outside
The Gargoyle will ask three questions. Possible questions are:
- Name: Your name as you entered it
- Quest: "Glory"
- Favorite color: "Purple"
- Whose spell protects the town: "Erana"
- Who do you seek: "Erasmus" or "Fenrus"
- Baron's first name: "Stefan"
- Thieves' Password: "I don't know"
Then enter the house.
3/177
Erasmus house
In the hallway look at the objects for some other Sierra game references, such as Police Quest, King's Quest 4, The Colonel's Bequest and Conquests of Camelot. Don't touch anything though and proceed up the stairs.




Answer yes to all Erasmus' questions and agree to play. SAVE THE GAME!
5/182
Mages Maze
You start at the upper left (the white player) and Erasmus' is the purple player in the upper right. Your goal is to get to the bottom right before Erasmus does. Use Fetch to move ladders and bridges around by dragging them. Use Open to move boulders. Use Flame Dart to create a miniature flame ball that attracts the creatures. Use Trigger to change the size of your player from small to medium to large and back to small again. Small creatures can pass through the small holes, and they move the fastest. Medium creatures can climb ladders and eat small creatures, but they move slower. Large creatures move the slowest, and eat medium creatures, but not small creatures. Like-sized creatures are attracted to each other. Use a magic potion when you're out of mana. Win the game and Erasmus teaches you the Dazzle Spell.
12/194
Erasmus room
Ask Erasmus about Erana, Baba Yaga, Zara, Warlock, Henry. Also, ask him about curse and countercurse.
Stand up and he'll send you to the bottom of the mountain.
1/195



BABA YAGA


Healer
Buy a potion of undead unguent for 100 silver


Market
Buy 50 apples from Hilde (5 silvers). This will overload the hero, considerably slowing him down.
3/198


Leave town, go 2 screens east, go north and east.


Brauggi
Say bargain and give the fruit. You'll get a glowing gem.
8/206


From the farm, go three screens west and two screens north.


Baba Yaga
Ask Bonehead (the gate skull) about eyes, deal and say "yes"
2/208


Ask Bonehead about the hut and the rhyme




Give the glowing gem and the gate will lower.
10/218


Type "hut of brown now sit down" (7) and enter (2).
9/227


Tell her your name, agree to help, and agree to go to the graveyard and get some Mandrake.


Woods
Wait until midnight (when it says "You are getting tired"), use the Undead Unguent (2) and head to the graveyard (left of town).
2/229
Graveyard
Get the little red plant (6) and return immediately to Baba Yaga.
6/235
Baba Yaga
Say yes and you'll give the mandrake to her
3/238



FREE BARNARD VON SPIELBURG


Ogre
The ogre, a unique enemy, is 1 south and 1 east from Erana's Peace. Fight and kill it.




Cast Open on the chest to open it. You'll get 1 gold and 43 silver.




Enter the cave.


Cave
Feed the bear with an apple vegetable or ration.
5/243


Talk twice to the bear for a Talking Bear Easter Egg




Walk past the bear into the next cave (2) and kill the kobold (10). Since the Kobold uses a counter spell, your flame darts will come right back at you. In my experience it's easier to attack him with the dagger.
12/255








Get the key
7/262


Cast detect to see an invisible chest at the bottom of the cave, cast open and get gold (10 gold and 60 silver).
5/267


Return to the bear and free bear
25/292
Castle
Enter the castle
10/302


Ask about brigands, brigand leader, warlock, prophecy, Elsa.
3/305



FREE ELSA VON SPIELBURG


Tavern
Get the note from underneath the stool (2 points if you didn't pick up the first note).
0/305


At noon, head for the archery south of town (from the town exit, 1 south and 1 west).


Archery
Witness Bruno's and Brutus' meeting. Remember the password: "Hiden Goseke".
12/317


Kill Brutus with flame darts.




Enter the archery along the south side (quick so Bruno doesn't spot you).




Go 1 south, 2 west and 1 south to the Antwerp (the "bouncer").


Antwerp
Go around the Antwerp, unlock the door with the key and open it.
10/327


Type "Hiden Goseke". Fred the Troll will go to the back of the cave.
5/332


Enter the cave.
2/334
Cave
Take the left tunnel to find Fred's treasure. Fight Fred, get his beard and search the pile (5 gold and 30 silver)




Return right and take the lower right exit.


Fortress Gate
Cast calm or fight Toro, the Minotaur, and search him.




Cast Open to open the gate, open the gate and enter the fortress.
8/342
Fortress courtyard
Go to the left, cross the right bridge, "step over rope" and go through the back right door.
8/350
Mess hall
SAVE game! Close door. Go to the upper right door and, as soon as the brigands start to leave, "move chair". Go to the candelabra and, as the first of three brigands walks behind the table, "move candelabra". Quickly walk in front of the table and climb table. Walk through the back left door.
8/358
Brigand Warlock
Ask about Elsa (2) or Yorick (8)
10/368


Walk through the first door on the left, then the first on the right. (If you start rolling, type "stop".) Pull the chain to open the back right door. Head back and go through it. Open the painted door, quickly move aside and, after it dropped down, enter it.
12/380
Brigand leader
Throw the Dispel Potion. It's Elsa Von Spielburg!
35/415


After Elsa disappears, go to the desk, get healing potions and get mirror.
10/425


Leave through the right side





BABA YAGA


Antwerp
Go 1 north, 1 west and 5 north


Baba Yaga
Bone head lets you in. Say "Hut of Brown, now sit down" and enter.




As you enter, immediately hold mirror and Baba Yaga will turn herself into a frog.
50/475
Castle
Reward ceremony
25/500


THE END
500/500


Thief

Location
Action
Points


INITIAL STEPS


Town entrance
You enter town
1/1


Ask the sheriff about brigands, magic, Otto, merchant, heros and monsters.
1/2
Hero's Tale Inn
Ask Shameen about the merchant (you learn his name is Abdulla and that he'll be there around suppertime), robbery, wealth, kattas and food.
1/3


Sit down (Shema enters) and order food. Eat the food.
1/4


Stand, exit, go west and enter the magic shop.


Magic shop
Ask about town, valley, aura, Erasmus, Erana, Baba Yaga, the curse, magic, potions, and spells.
1/5


Exit and enter the guild hall to the left.


Guild hall
Read the Adventurer's logbook (4) and sign it (1).
5/10


Look at the board.
6/16


Read the individual quests. There are 6 of them:
- Reward for return of lost ring. Inquire at the Healer's.
- Reward of 50 gold coins for the safe return of Elsa von Spielburg. Inquire at Spielburg Castle gates.
- Reward of 30 gold coins for the capture or death of the Brigand Warlock. Description: short, ugly, and wears brightly colored robes. Has habit of laughing continually. Inquire at Spielburg Castle gates.
- Wanted: Brigand Leader. Description: Unknown appearance, wears a cloak. Must provide proof of leader's identity. Reward of 60 gold coins and title of Hero of the Realm. Inquire at Spielburg Castle gates.
- Notice: Spell components needed. Cash or trade for potions. Inquire at the Healer's.
- Reward of 50 gold coins for information leading to the return of Baronet Barnard von Spielburg. Inquire at Spielburg Castle gates.




Wake up the Guildmaster, Wolfgang, and question him about Barnard, Elsa, Yorick, monsters, brigands, Baba Yaga and the curse.
1/17


Leave, go past the sheriff's office and north to Market Street.


Dry goods store
Ask Kaspar about equipment and armor.
1/18


As a thief, you'll need to buy daggers here. They cost 20 silver a piece. Buy 2 more daggers for starters.




Exit, go west to Tavern Street and enter the alley.


Alley
Give Sam the beggar one silver.
1/19


Ask Sam about the night and brigands.
1/20
Tavern
Get the note from underneath the stool.
2/22
Market
Ask Hilde the centaur about Brigands, fruit, vegetables, father.
1/23


Leave and wander in town (don't leave town yet!) until sunset. Then enter the inn.


Hero's Tale Inn
Sit and ask Abdulla about brigands, the brigand leader, minotaur, his name, and Shapeir.
5/28


Give Abdulla a ration. He'll tell you about his magic carpet, which was among the stolen items.
2/30


Go to Shameen and pay room`(5 silvers).
1/31


Leave town (1), and go all the way east to the avalanche. Then travel west and back east until you see a trapped fox.
1/32
Trapped fox
Free fox and he'll give information on how to break Baba Yaga's curse.
10/42


Head back outside the town entrance and one north to the healer. Knock on the door and enter.


Healer
Ask about potions, components, and her ring.
2/44


Go outside and climb the tree (until you succeed). Get ring. If agility isn't high enough, you'll fall and lose some health points. In that case, get rocks and throw rocks until it falls and you recover the ring.
3/47


Return inside and give the ring to the healer. You'll get six golds and two Healing Potions
10/57


Exit and go one screen west to the farm.


Farm
Ask Heinrich about the centaur about his name, his daughter Hilde, the farm, and the brigands (1). Also ask him about the leader (3).
4/61


Go east and north to the castle gate.


Castle gate
Ask Karl the Gatekeeper about the castle, the baron, Barnard, Elsa, Baba Yaga, and the curse.
5/66


Open gate and enter the courtyard.
1/67
Courtyard
If the weapon Master is there, ask him about fighting.
1/68


Rest a few times to increase stamina




Go right to the stables.


Stables
Enter the stables and type "yes" when asked to work. You'll get 5 silver coins.
5/73



BUILDING UP THE STATS


Easy combat
From here on you'll run into monsters. At first make sure to wander around during the day (during the night, you'll run into deadlier foes until you reach 1000 experience, after which they also appear during the day) and kill random creatures (save regularly). Throw daggers as the monster approaches to weaken it, but make sure to keep at least one dagger for close combat! Rest after each fight (repeat until the hero refuses to rest more). You'll run into three different foes: Goblins, Purple Saurus and Brigands. Goblins are the easiest. You may also want to try fighting a Purple Saurus. Avoid the Brigand until your stats are high enough (30 or above). Search the bodies of goblins and brigands each time for loot (Purple Saurus doesn't have any) and to retrieve your daggers. When health gets low, have a healing potion (you'll get an empty flask after use) or avoid combat until the next day after you've recovered at Erana's Peace. You can buy additional healing potions at the healer for 40 silvers a piece.


Throwing skill
Enhance your throwing skill to 100 by going in the woods, getting rocks from the ground and throwing them away (no target needed).


Stealth
Switch between walking and sneaking a lot to increase your stealth skill to 100.


Lock picking
Pick the lock of the Healer's hut. Since it's barricaded, you can never enter, so you can also lock pick during the day to boost this stat. Once the lock picking skill is above 50, you can pick your nose (no joke) until the stat reaches 100.


Climbing
Climb up and down the tree at the Healer's hut to increase this stat to 100. During the night, you can also climb the castle wall and town gate.


Castle
Feel free to clean the stables daily for extra 5 silvers.


Goblin camp
From the farm go 3 west to find a group of goblins. These are excellent for combat practice and money. You start fighting 1 goblin and each time you fight there again, you'll fight an extra goblin. Search all bodies to earn 56 silvers each time.


Erana's Peace
At the end of each day, you can eat and sleep (and consequently recover health and stamina) at Erana's Peace. To get there, go from the farm, four screens north, one east, and one north. Run to avoid monsters.




Eat fruit from the tree each day to avoid using rations (2 points the first time). Sleep.
2/75
Hard combat
When experience exceeds 1000, tougher opponents will appear. The Cheetaur and Mantray, which previously only appeared during the night, now also appear during the day. Make sure to get the Cheetaur's claws after killing it. A new monster is the Saurus Rex, which appears both day and night. During nighttime only, the troll appears. Search its body and get its beard.


Dry goods store
If you have 500 silver (1 gold is 10 silver), buy armor.





THE THIEVES GUILD


Alley
At night, go to the back of the alley where Sam the beggar was (you can climb over the wall at the town entrance).




Make thief sign and you get the password. Remember it because it is random! Passwords can be: Schwertfisch, Ach du lieber, Schweinhund, Antwerp, Rheingold, Deutschmark, or Purple Saurus.
3/78
Tavern
Tell Crusher (the guy sitting at the left) the password and he'll let you into the Thieves' Guild.
5/83
Thieves Guild
Buy a license from Boris for 25 silvers.
3/86


Buy a Thieves Tool Kit for 100 silvers.
3/89


Ask about game and play game (3). Wager at least 25 silvers and win (5).
8/97


Leave by the ladder


Old lady's house
At night, pick the lock of the house next to the magic shop and enter.
5/102


Search the desk at the left wall for silvers (1), the couch for silvers (1), open the purse on the couch for silvers (1), search the basket for the string of pearls (1), and take the silver candlesticks (1) on the table.
5/107


Pet (or feed) the cat.
3/110


Search the cage


Sheriff's house
Pick the lock of the Sheriff's house (next to the dry goods shop) and enter.
5/115


Get the candelabra off the desk (1) and search the desk for silver (1).
2/117


Open the music box on the table and get it.
1/118


Get the vase on the mantle (1), move the painting (1), unlock the safe (1), and get the bag of silver (1).
4/122
Thieves guild
Sell the candlesticks (50 silvers), pearls (100 silvers), music box (30 silvers), vase (40 silvers) and candelabra (75 silvers) to Boris.
3/125


Return to Erana's Peace for sleep.





DISPEL POTION


Erana's Peace
After a good night sleep at Erana's Peace, pick flowers.


Town entrance
From the town entrance, go 2 south and 3 west, and follow the stag. Don't hurt it!


Dryad
Approach the tree and the Dryad will appear, asking if you're a friend of the forest. Answer yes and the Dryad will ask to bring a seed from the Spore Spitting Spirea in the north.
1/126


Go 1 east and one north.


Mushroom circle
Get mushrooms. (feel free to eat them, but save first).
3/129


Go 1 east, 4 north, 1 east and 1 north.


Spore Spitting Spirea
Climb the rocks and catch the seed or throw rock at seed.
8/137
Dryad
Answer yes and you'll give the seed (7). The Dryad will tell you of a dispel potion the healer can make for you. It requires flowers from Erana's Peace, Green fur, Fairy Dust, a Magic Acorn, and flying water.
7/144


Get the acorn that falls from the tree.
1/145


Go 1 east, 2 north and 2 west.


Meeps
Talk to meeps and ask about meeps, fur and green fur.
1/146


Get the green fur
5/151


Go 2 east, 2 south, and 5 east


Hermit
Get water (requires empty flask)
3/154


Climb the rocks (alternatively, throw 3 rocks at the door. The hermit will come out and show you the ladder. Climb it)




Stand right off the door and knock to get in.
6/160


Ask about hermits, ladder, cave, magic, brigands, spell, scroll (if you have magic skill).
2/162


Leave the hermits house and climb down the ladder. Then go 1 west and 1 south.


Mirror Lake
Just for entering the screen, you get 1 point.
1/163


Exit and re-enter for a Code Name: Iceman Easter egg.


Town entrance
If Bruno is there (he appears from midday), try asking him something. Give him a silver, and another silver. Ask about Baba Yaga about the Castle, and goblins. Don't give money for more info. You get 2 points when you leave the screen.
2/165
Healer
Give acorn
5/170


Give green fur
2/172


Give flowers. You'll get 5 silvers.
1/173


Give flying water.
2/175


Give mushrooms. You'll get one gold.
1/176


Give Sheetaur claws for 5 silver each.
2/178


Give troll beard. You get 2 healing potions for each.
2/180
Mushroom ring
Make sure you have an empty flask. At night the fairies will be there. Answer yes when they ask you to dance.
3/183


Ask about fairies, dryad and fairy dust, and you'll get some.
9/192
Healer
Give fairy dust
2/194


Exit and re-enter and you'll get the dispel potion
7/201



ERASMUS


Healer
From the healer, go east, north, and twice east. You'll see Erasmus' house in the distance. Read the boards and go up the mountain.


Erasmus outside
The Gargoyle will ask three questions. Possible questions are:
- Name: Your name as you entered it
- Quest: "Glory"
- Favorite color: "Purple"
- Whose spell protects the town: "Erana"
- Who do you seek: "Erasmus" or "Fenrus"
- Baron's first name: "Stefan"
- Thieves' Password: "I don't know"
Then enter the house.
3/204
Erasmus house
In the hallway look at the objects for some other Sierra game references, such as Police Quest, King's Quest 4, The Colonel's Bequest and Conquests of Camelot. Don't touch anything though and proceed up the stairs.
In Erasmus tower room, ask him about Erana, Baba Yaga, Zara, Warlock, Henry. Also, ask him about curse and countercurse.
Stand up and he'll send you to the bottom of the mountain.
1/205



BABA YAGA


Healer
Buy a potion of undead unguent for 100 silver


Market
Buy 50 apples from Hilde (5 silvers). This will overload the hero, considerably slowing him down.
3/208


Leave town, go 2 screens east, go north and east.


Brauggi
Say bargain and give the fruit. You'll get a glowing gem.
8/216


From the farm, go three screens west and two screens north.


Baba Yaga
Ask Bonehead (the gate skull) about eyes, deal and say "yes"
2/218


Ask Bonehead about the hut and the rhyme




Give the glowing gem and the gate will lower.
10/228


Type "hut of brown now sit down" (7) and enter (2).
9/237


Tell her your name, agree to help, and agree to go to the graveyard and get some Mandrake.


Woods
Wait until midnight (when it says "You are getting tired"), use the Undead Unguent (2) and head to the graveyard (left of town).
2/239
Graveyard
Get the little red plant (6) and return immediately to Baba Yaga.
6/245
Baba Yaga
Say yes and you'll give the mandrake to her
3/248



FREE BARNARD VON SPIELBURG


Ogre
The ogre, a unique enemy, is 1 south and 1 east from Erana's Peace. Fight and kill it.




Pick the lock of the chest. You'll get 1 gold and 43 silver.




Enter the cave.


Cave
Feed the bear with an apple vegetable or ration.
5/253


Talk twice to the bear for a Talking Bear Easter Egg




Walk past the bear into the next cave (2) and fight the kobold (you can throw daggers, but make sure to keep one for close combat).
12/255








Get the key
7/262


Walk at the very bottom of the cave until you bump into something invisible. pick lock and get gold (unlike the fighter, you'll avoid the trap).
5/267


Return to the bear and free bear
25/292
Castle
Enter the castle
10/302


Ask about brigands, brigand leader, warlock, prophecy, Elsa.
3/305



FREE ELSA VON SPIELBURG


Tavern
Get the note from underneath the stool (2 points if you didn't pick up the first note).
0/300


At noon, head for the archery south of town (from the town exit, 1 south and 1 west).


Archery
Witness Bruno's and Brutus' meeting. Remember the password: "Hiden Goseke".
12/317


Enter the archery along the south side (quick so Bruno doesn't spot you).




Kill Brutus and get the key.




Go 1 south, 2 west and 1 south to the Antwerp (the "bouncer").


Antwerp
Go around the Antwerp, unlock the door with the key and open it.
10/327


Type "Hiden Goseke". Fred the Troll will go to the back of the cave.
5/332


Enter the cave.
2/334
Cave
Take the left tunnel to find Fred's treasure. Fight Fred, get his beard and search the pile (5 gold and 30 silver)




Return right and take the lower right exit.


Fortress Gate
Sneak by or defeat and search the body of Toro.




At the right side, climb over the gate and enter the fortress.
8/342
Fortress courtyard
Go to the left, cross the right bridge, "step over rope" and go through the back right door.
8/350
Mess hall
SAVE game! Close door. Go to the upper right door and, as soon as the brigands start to leave, "move chair". Go to the candelabra and, as the first of three brigands walks behind the table, "move candelabra". Quickly walk in front of the table and climb table. Walk through the back left door.
8/358
Brigand Warlock
Ask about Elsa (2) or Yorick (8)
10/368


Walk through the first door on the left, then the first on the right. (If you start rolling, type "stop".) Pull the chain to open the back right door. Head back and go through it. Open the painted door, quickly move aside and, after it dropped down, enter it.
12/380
Brigand leader
Throw the Dispel Potion. It's Elsa Von Spielburg!
35/415


After Elsa disappears, go to the desk, get healing potions and get mirror.
10/425


Leave through the right side





BABA YAGA


Antwerp
Go 1 north, 1 west and 5 north


Baba Yaga
Bone head lets you in. Say "Hut of Brown, now sit down" and enter.




As you enter, immediately hold mirror and Baba Yaga will turn herself into a frog.
50/475
Castle
Reward ceremony
25/500


THE END
500/500


