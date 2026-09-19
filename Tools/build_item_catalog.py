"""Turns the item survey into the item catalogue.

    python Tools/build_item_catalog.py

Reads RawArt/item_survey.json (Tools/survey_items.py: every pack prop that could be carried,
with its measured size and category) and writes UI/Items.json, the Reference page's second
catalogue: { "items": { "<Pack>/<Name>": { name, category, pack, mesh, icon, description, size,
keep } } }. Re-runnable: an entry whose keep is true (set when its name or description is saved
from the Reference page) keeps those two fields; everything else is regenerated from the survey.
Icons are T_Icon_<Pack>_<Name>, rendered by Tools/render_item_icons.py.
"""
import io, json, os, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SURVEY = os.path.join(ROOT, 'RawArt', 'item_survey.json')
CAT = os.path.join(ROOT, 'Content', 'GameData', 'UI', 'Items.json')

accepted = json.load(io.open(SURVEY, encoding='utf-8'))['accepted']
old = json.load(io.open(CAT, encoding='utf-8')).get('items', {}) if os.path.exists(CAT) else {}
items = {}
for r in accepted:
    short = re.sub(r'^SM_(Prop|Item|Att|Chr_Attach|Attach)_', '', r['name'])
    key = r['pack'] + '/' + short
    name = ' '.join(w for w in short.split('_') if w)
    prev = old.get(key, {})
    keep = bool(prev.get('keep'))
    # Everything authored on the old entry survives (Tools/backfill_item_fields.py and the
    # Reference page write dozens of fields); only the MEASURED fields are regenerated, and the
    # name is regenerated until someone has saved it (keep).
    e = dict(prev)
    e.update({
        'name': prev.get('name', name) if keep else name,
        'category': prev.get('category', r['category']) if keep else r['category'],
        'pack': r['pack'],
        'mesh': r['path'],
        'icon': re.sub(r'[^A-Za-z0-9]+', '_', r['pack'] + '_' + short).strip('_'),
        'description': prev.get('description', ''),
        'size': r['size'],
        'keep': keep,
    })
    items[key] = e
# Entries made by hand (Tools/make_garments and the like: no pack asset behind them) are kept as they are.
for key, prev in old.items():
    if prev.get('handmade') and key not in items: items[key] = prev
doc = {
    '_': 'Things a character can carry, taken from the packs by Tools/survey_items.py and written by Tools/build_item_catalog.py. '
         'category is armor | equipment | consumables | other; mesh is the pack asset; icon names T_Icon_<icon>. '
         'name and description are yours once keep is true (the Reference page sets it on save); everything else is regenerated.',
    'items': dict(sorted(items.items())),
}
io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(doc, indent=1, ensure_ascii=False))
from collections import Counter
print('wrote %d items to %s' % (len(items), CAT))
print(dict(Counter(e['category'] for e in items.values())))
