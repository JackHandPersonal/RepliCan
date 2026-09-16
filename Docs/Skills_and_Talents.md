# Skills and Talents

A first pass at the character progression layer for RepliCan. Everything here is written
against systems the game already has (the weapon catalogue's `spread_hip`, `spread_aim`,
`recoil`, `reload_s`, `durability_max`, `bulk`, `value`; the inspect actions Hack, Press Button,
Open, Take, Sit; the light panels, the lift, conversations, the replicant contract) so each
line names the number or hook it would move. Nothing here is implemented yet.

**Skills** are trained by use and by teaching, rated 0..10, and scale numbers. **Talents** are
special abilities: picked at milestones, mostly binary, and they change rules rather than
numbers. A talent usually has a skill prerequisite.

The fiction to keep in mind: the player is a decanted replicant on a PCS (Prompt Critical
Services) contract, thirteen thousand in debt to Helion Credit, on a field station bolted to a
rock nobody wanted. The recorder missed the last few minutes before the boarding. Nobody here
is a soldier by trade; they are salvage crews, contract hunters, mech pilots and station staff
who own weapons because the station is the kind of place where you do.

## Skills

### Combat

| Skill | What it covers | What it moves |
| --- | --- | --- |
| Sidearms | Pistols, revolvers, machine pistols, stun guns | `spread_hip`/`spread_aim` and `recoil` scale down with rank; draw and holster time |
| Long Guns | Rifles, SMGs, shotguns, sniper rifles | as above; ADS settle time; the shoulder point's recoil control kicks in earlier |
| Heavy Weapons | Launchers, flamethrowers, the mining laser used as a weapon | movement penalty while carrying; reload_s; blast placement |
| Energy Weapons | Laser guns, plasma shotguns, laser cutters, beam rifles | charge drain per shot, overheat window, `range_m` |
| Blades | Daggers, swords, machetes, the shock stick | swing speed, guard, damage multiplier, throwing knives |
| Close Quarters | Unarmed, grapples, weapon butt strikes | stagger chance, disarm, breaking a hold |
| Thrown | Shurikens, grenades, anything with `use: throw` | arc accuracy, throw distance |

### Body

| Skill | What it covers | What it moves |
| --- | --- | --- |
| Athletics | Sprinting, vaulting, climbing, mantling | sprint stamina cost, vault height, fall damage threshold |
| Endurance | Carrying, wounds, exhaustion | encumbrance limit from `mass_kg`, wound penalties, recovery rate |
| Stealth | Moving unheard and unseen | footstep noise, detection radius, crouch speed |
| EVA | Zero-g and vacuum work, suit discipline | suit air use, tether handling, pushing off in zero-g |

### Technical

| Skill | What it covers | What it moves |
| --- | --- | --- |
| Hacking | Terminals, the Hack inspect action, door locks, cameras | which locks are attempted, time to crack, alarm chance |
| Electronics | Panels, lights, the lift, wiring, jury-rigged power | which panels can be re-wired (a `toggle:` on something new), repair of electrical gear |
| Mechanics | Weapons, mechs, doors, pumps, the decant tank | `durability_max` loss rate, field repair, jam clearing |
| Salvage | Stripping wrecks, cutting, sorting scrap | extra loot from containers and bodies, cutting time with a laser cutter, what counts as junk |
| Demolitions | Charges, breaching, pressure doors | placing and defusing, breach damage, blast radius safety |
| Piloting | Mechs, the station's tugs, whatever flies | mech handling, docking, transit time between sites |
| Medicine | Trauma, stims, the decant tank | heal effect strength (`effects: heal 25`), bleed control, revive |
| Chemistry | Stims, propellants, solvents, drugs | crafting consumables, ammo reloading, identifying unknowns |

### Mind and Social

| Skill | What it covers | What it moves |
| --- | --- | --- |
| Perception | Noticing, tracking, reading a room | Inspect reveals more (hidden `desc:` lines), spotting traps and cameras, tracer trails |
| Persuasion | Talking people into things | conversation branches open, prices, favours |
| Intimidation | Making the other option look worse | branches open, hostile disengagement |
| Deception | Lying, forging, impersonating staff | branches open, fake credentials on doors and terminals |
| Barter | Buying, selling, valuing | `value` on sale and purchase, what a vendor will take |
| Streetwise | Contacts, rumours, who owes whom | leads, safe routes, black-market access |
| Protocol | Corporate procedure: PCS forms, Helion terms, station regs | official channels open, penalties reduced, permissions granted without a favour |

### Replicant

| Skill | What it covers | What it moves |
| --- | --- | --- |
| Fidelity | How much of the recorder the mind can still read | recovering lost memories (story gates), resistance to the gaps |
| Sync | Staying within download range and keeping the backup current | how far from the station the contract still covers, what is lost on a rollback |
| Maintenance | Looking after the decanted body | stim tolerance, wound healing, the ceiling on Endurance |

## Talents

Talents are picked, not trained. Most are binary. Where a talent needs a rank it is given as
`(Skill n)`.

### Gunplay

- **Trigger Discipline** (Sidearms 3 or Long Guns 3): the first shot of a burst gets no recoil kick; the rest kick as normal.
- **Steady Shoulder** (Long Guns 4): stocked weapons recover recoil twice as fast; the shoulder point is where this lives.
- **Hip Shooter** (Sidearms 4): hip-fire spread halved; hip-fire recoil scale becomes the shouldered scale.
- **Quick Hands** (Sidearms 2): weapon swap and holster in half the time; reload 25% faster.
- **Selector Savvy** (Long Guns 2): fire-mode change is free, even mid-burst; the first round in auto is as accurate as a semi shot.
- **Dead Eye** (Long Guns 6): after a full second in ADS the next shot has zero spread.
- **Two Hands** (Sidearms 5): a pistol in each hand; fires both, no ADS.
- **Point Blank** (Close Quarters 3): shots within two metres do 50% more damage and stagger.
- **Cold Barrel** (Energy Weapons 3): the first shot from a cooled energy weapon does double damage.
- **Overcharge** (Energy Weapons 5): hold the trigger to charge past the limit for one heavy shot at the cost of the weapon's durability.

### Movement and Body

- **Vaulter** (Athletics 3): mantles chest-high cover; vaults without losing sprint.
- **Cat Feet** (Stealth 3): no footstep sound at walk; landing from a drop makes no noise.
- **Low Profile** (Stealth 4): crouched movement at walking speed; harder to spot in the dark.
- **Second Wind** (Endurance 3): once per fight, stamina refills when it hits zero.
- **Thick Skin** (Endurance 5): wound penalties halved.
- **Pack Mule** (Endurance 2): +4 inventory cells (`bulk`), and heavy weapons carry with no speed penalty.
- **Iron Lungs** (EVA 3): suit air lasts half again as long; no panic in vacuum.
- **Sure Footing** (EVA 4): zero-g movement uses no stamina; no tumbling after a push.

### Technical

- **Backdoor** (Hacking 4): every lock rated at or below the skill opens instantly, no minigame.
- **Ghost in the Wire** (Hacking 6): hacked cameras and turrets ignore the player for two minutes.
- **Power Route** (Electronics 3): any light panel in a section can control every fixture in that section, not just its own group.
- **Lift Override** (Electronics 4): call the lift from any panel; floors marked out of service become reachable.
- **Jury-Rig** (Mechanics 3): field repair with scrap up to 60% durability, no bench needed.
- **Field Strip** (Mechanics 4): break a weapon into parts (its optic, mag, accessories) that go into inventory separately.
- **Scavenger's Eye** (Salvage 2): loot containers and bodies are outlined at range; junk is auto-sorted.
- **Cutter's Patience** (Salvage 4): the laser cutter opens sealed containers and locked doors without an alarm.
- **Shaped Charge** (Demolitions 3): breach charges open bulkheads; blast does not damage the player.
- **Steady Stick** (Piloting 3): mechs move at full speed while firing.
- **Field Surgeon** (Medicine 4): revive a downed companion; heals apply over time instead of all at once, so they are not wasted.
- **Home Brew** (Chemistry 3): stims and ammo craftable from station stock; recipes revealed by inspecting ingredients.

### Mind and Social

- **Cold Read** (Perception 3): an NPC's disposition shows in the inspect menu before a word is said.
- **Tracker** (Perception 4): footprints and blood trails show for a minute after they are made.
- **Company Voice** (Protocol 3): speaking as PCS opens staff-only doors and terminals; costs standing if abused.
- **Silver Tongue** (Persuasion 4): a failed persuasion can be retried once in the same conversation.
- **Bad Cop** (Intimidation 4): a drawn weapon counts as an argument; hostile groups may back down before shooting.
- **Straight Face** (Deception 4): lies are not flagged as lies in the conversation log; forged credentials pass a second check.
- **Haggler** (Barter 3): sell at 20% more, buy at 20% less.
- **Well Connected** (Streetwise 3): one contact per district who will hide the player and hold goods.

### Replicant

- **Rollback** (Sync 2): on death, restore from the last download and lose only the minutes since, not the day.
- **Long Leash** (Sync 4): the contract's download range doubles.
- **Muscle Memory** (Fidelity 3): the recorder's skills carry over: start with three ranks the previous body had.
- **Eidetic Buffer** (Fidelity 4): every code, name and map read once is kept in the journal, verbatim.
- **Pain Editor** (Maintenance 3): wound penalties off for thirty seconds on command; the damage is still real.
- **Redline** (Maintenance 4): ten seconds of doubled sprint and reload speed, paid for in durability of the body (a wound afterwards).
- **Sleepless** (Maintenance 2): no fatigue; night shifts and long EVAs cost nothing.
- **Gap Filler** (Fidelity 5): the decant left holes; this talent lets the player choose what was in one of them, once, when it matters (a story unlock).

## Suggested first cut

If only six of each ship first: Sidearms, Long Guns, Athletics, Hacking, Salvage, Persuasion;
Trigger Discipline, Quick Hands, Backdoor, Scavenger's Eye, Cold Read, Rollback. They all sit on
numbers and hooks the game has today.
