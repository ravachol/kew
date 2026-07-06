About the artist data:

- This dataset contains artist names and homepage URLs sourced from Wikidata.

- Wikidata is licensed under the Creative Commons Attribution-ShareAlike 4.0 International License (CC BY-SA 4.0).
https://creativecommons.org/licenses/by-sa/4.0/.

- The artists.tsv is not used by kew. It is included in the repository for reference.

- The data is extracted from wikidata available at:
https://dumps.wikimedia.org/wikidatawiki/entities/latest-all.json.bz2

- The binary file, artists.db, is needed to display homepage links for artists in kew.

- kew works fine even if the file isn't available.

- The script extract_from_wikidata.py is used to extract the artist data from the bz2 file.

- The script keep_only_matching_urls.py removes domains urls that don't have words from the artist in them as these are deemed to be erroneous data for the most part.

- The script apply_corrections.py washes the data based on the information in corrections.tsv.

- The script verify_urls.py is used to verify that urls work.

- The script make_binary_db.py is used to create artists.db from artists.tsv.

To recreate the same file run: 'python make_binary_db.py artists.tsv artists.db' using Python 3.14.5.
