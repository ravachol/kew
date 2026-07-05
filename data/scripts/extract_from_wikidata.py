import bz2
import json

INPUT = "latest-all.json.bz2"
OUTPUT = "artists_raw.tsv"

# QIDs
Q_HUMAN = "Q5"
Q_BAND = "Q215380"
Q_MUSICAL_ARTIST = "Q639669"

# properties
P_INSTANCE_OF = "P31"
P_OCCUPATION = "P106"
P_WEBSITE = "P856"

def get_claim_values(entity, pid):
    claims = entity.get("claims", {}).get(pid, [])
    values = []
    for c in claims:
        try:
            v = c["mainsnak"]["datavalue"]["value"]
            if isinstance(v, dict) and "id" in v:
                values.append(v["id"])
        except:
            continue
    return values

def get_website(entity):
    claims = entity.get("claims", {}).get(P_WEBSITE, [])
    for c in claims:
        try:
            v = c["mainsnak"]["datavalue"]["value"]
            if isinstance(v, str):
                return v
        except:
            continue
    return None

with bz2.open(INPUT, "rt", encoding="utf-8") as f, \
     open(OUTPUT, "w", encoding="utf-8") as out:

    out.write("qid\tname\twebsite\n")

    for line in f:
        line = line.strip().rstrip(",")

        if not line.startswith("{"):
            continue

        try:
            item = json.loads(line)
        except:
            continue

        qid = item.get("id")
        if not qid:
            continue

        # ---- filter: human or band ----
        instance_of = get_claim_values(item, P_INSTANCE_OF)
        occupation = get_claim_values(item, P_OCCUPATION)

        is_artist = (
            Q_HUMAN in instance_of or
            Q_BAND in instance_of or
            Q_MUSICAL_ARTIST in occupation
        )

        if not is_artist:
            continue

        # ---- official website ----
        website = get_website(item)
        if not website:
            continue

        # ---- label ----
        label = item.get("labels", {}).get("en", {}).get("value", "")

        out.write(f"{qid}\t{label}\t{website}\n")

print("DONE →", OUTPUT)
