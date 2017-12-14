# ubl batch compound
===========================

## Introduction

With this module it is possible to add compound items in batch. You can add children to an existing compound, or you can add children to a non-existing compound that is generated as needed.
This module has a web interface to handle small (up to about 200 items) batches and drush commands for bigger batches.

## Web interface
Go to the URL https://[islandora-server]/ubl_batch_compound and upload a CSV file. See below for the format of the CSV file.
Once the CSV file is uploaded, a list of potential compound items is displayed, so a review is possible before making changes. Items that are mistakenly found, can be deselected.
After the review the compound items can be made. Then the changes can be reviewed by clicking on the link of the parent items.

## Drush

Because the generation of large amounts of compounds can take a long time, there are also drush commands to do the same:

### `ubl_batch_compound_prepare`

Prepare a batch compound CSV file. Based on an existing csv file, this will insert rows with the actual PIDs of the children (and parents if they already exist) and print the new CSV file to output. Then this CSV file can be used with the `ubl_batch_compound_generate` drush command.

This command can have the following options:
 - `csvfile`: Mandatory, the absolute filepath of a CSV file. See below for the exact format.
 - `solrfield`: Optional, specify the Solr field that should be used to search the compound and children by. For example use `mods_identifier_local_s` to only search by identifier local.
 - `exactmatch`: Optional, only do exact matches. False by default.

Examples:
 - `drush --user=admin ubl_batch_compound_prepare --csvfile=/path/to/directory/with/csv_file.csv > /another/path/new_csv_file.csv`
 - `drush --user=admin ubc_prepare --csvfile=/path/to/directory/with/csv_file.csv > /another/path/new_csv_file.csv`
 - `drush --user=admin ubc_prepare --csvfile=/path/to/directory/with/csv_file.csv --solrfield=mods_identifier_local_s --exactmatch > /another/path/new_csv_file.csv`
  
### `ubl_batch_compound_generate`

Generate the compounds by making the compound parents (if needded) and adding the children.
The children of the CSV file must be identified by PID. The parent can be the value of the label for newly generated compound parents, or a PID for existing compound parents.

Use the output of the `ubl_batch_compound_prepare` drush command as the input for this command AFTER you have checked if the CSV file is correct.

This command must have the following option:
 - `csvfile`: Mandatory, the absolute filepath of a CSV file. See below for the exact format. The second column (children) should only contain PIDs, the first column can contain labels (for new compounds) or PIDs (for existing compounds).

Examples:
 - `drush --user=admin ubl_batch_compound_generate --csvfile=/path/to/directory/with/csv_file.csv`
 - `drush --user=admin ubc_generate --csvfile=/path/to/directory/with/csv_file.csv`

## Format of CSV file

This file contains (at least) two columns:
 - The values of the first column are used to search for potential parent items. This can be a label (to use for a new compound parent), a search string (an identifiying string inside an existing compound parent) or a PID (of an existing compound parent). If none or more than one item is found, a new parent is assumend; this parent will get this value as its label.
 - The values in the second column are used to find the child items. This can be a search string (zero or more children can be found) or a PID.

## License

[GPLv3](LICENSE.txt)
Copyright 2017 Leiden University Library

