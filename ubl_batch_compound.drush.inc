<?php
/**
 * @file
 * Drush functions.
 *
 *
 *  Copyright 2017 Leiden University Library
 *
 *  This file is part of ubl_batch_compound.
 *
 *  ubl_batch_compound is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * Implements hook_drush_command().
 */
function ubl_batch_compound_drush_command() {
  $items['ubl_batch_compound_prepare'] = array(
    'description' => 'Prepare a batch compound CSV file. Based on an existing csv file, this will insert rows with the actual PIDs of the children (and parents if they already exist) and print the new CSV file to output. Then this CSV file can be used with the ubl_batch_compound_generate drush command.',
    'options' => array(
      'csvfile' => 'Mandatory, the absolute filepath of a CSV file. See documentation for the exact format.',
      'solrfield' => 'Optional, specify the Solr field that should be used to search the compound and children by. For example use "mods_identifier_local_s" to only search by identifier local.',
      'exactmatch' => 'Optional, only do exact matches. False by default.',
    ),
    'aliases' => array('ubc_prepare'),
    'examples' => array(
      'drush --user=admin ubl_batch_compound_prepare --csvfile=/path/to/directory/with/csv_file.csv > /another/path/new_csv_file.csv',
      'drush --user=admin ubc_prepare --csvfile=/path/to/directory/with/csv_file.csv > /another/path/new_csv_file.csv',
      'drush --user=admin ubc_prepare --csvfile=/path/to/directory/with/csv_file.csv --solrfield=mods_identifier_local_s --exactmatch > /another/path/new_csv_file.csv',
    ),
  );
  $items['ubl_batch_compound_generate'] = array(
    'description' => 'Generate the compounds based on a CSV file. The children of the CSV file must be identified by PID. The parent can be the value of the label for newly generated compound parents, or a PID for existing compound parents.',
    'options' => array(
      'csvfile' => 'Mandatory, the absolute filepath of a CSV file, containing PIDs for the children. See documentation for the exact format.',
    ),
    'aliases' => array('ubc_generate'),
    'examples' => array(
      'drush --user=admin ubl_batch_compound_generate --csvfile=/path/to/directory/with/csv_file.csv',
      'drush --user=admin ubc_generate --csvfile=/path/to/directory/with/csv_file.csv',
    ),
  );
  return $items;
}

/**
 * Implements drush_hook_COMMAND_validate().
 */
function drush_ubl_batch_compound_prepare_validate() {
  $csvfile = drush_get_option('csvfile');
  if (substr($csvfile, 0, 1) !== DIRECTORY_SEPARATOR) {
    return drush_set_error("csvfile should be an absolute path to an existing CSV file, but is not absolute.");
  }
  if (!is_file($csvfile)) {
    return drush_set_error("csvfile should be an absolute path to an existing CSV file, but does not exist.");
  }
  return TRUE;
} 

/**
 * Implements drush_hook_COMMAND().
 */
function drush_ubl_batch_compound_prepare() {
  module_load_include('inc', 'ubl_batch_compound', 'includes/utilities');
  $csvfile = drush_get_option('csvfile');
  $solrfield = drush_get_option('solrfield');
  $exactmatch = drush_get_option('exactmatch', FALSE);

  $result = '';
  
  $parent2child = ubl_batch_compound_read_csv($csvfile, 'drush_log', FALSE, $solrfield, $exactmatch);

  foreach ($parent2child as $parent => $info) {
    if (isset($info['children'], $info['enabled'], $info['childselcount']) && $info['enabled'] && $info['childselcount'] > 0) {
      $parentthing = '';
      if (isset($info['usesingleparent'], $info['singleparentid']) && $info['usesingleparent']) {
        $parentthing = $info['singleparentid'];
      }
      else {
        $parentthing = $parent;
        drush_log(t("No existing compound found for parent identifier '@parent'", array("@parent" => $parent)), "warning");
      }
      foreach ($info['children'] as $childlabel => $childs) {
        if (count($childs) > 1) {
          drush_log(t("Found multiple children for child identifier '@childlabel'", array("@childlabel" => $childlabel)), "warning");
        }
        foreach ($childs as $cid => $enabled) {
          if ($enabled) {
            $reltype = empty($info["childrentypeinfo"][$cid]["type"])?"":$info["childrentypeinfo"][$cid]["type"];
            $relvalue = empty($info["childrentypeinfo"][$cid]["info"])?"":$info["childrentypeinfo"][$cid]["info"];
            $result .= "$parentthing;$cid;$reltype;$relvalue;$parent;$childlabel\n";
          }
        }
      }
    }
  }
  print $result;
}

/**
 * Implements drush_hook_COMMAND_validate().
 */
function drush_ubl_batch_compound_generate_validate() {
  module_load_include('inc', 'ubl_batch_compound', 'includes/utilities');

  $csvfile = drush_get_option('csvfile');
  if (substr($csvfile, 0, 1) !== DIRECTORY_SEPARATOR) {
    return drush_set_error("csvfile should be an absolute path to an existing CSV file, but is not absolute.");
  }
  if (!is_file($csvfile)) {
    return drush_set_error("csvfile should be an absolute path to an existing CSV file, but does not exist.");
  }

  $parent2child = ubl_batch_compound_read_csv($csvfile, 'drush_log', TRUE);

  foreach ($parent2child as $parent => $info) {
    if (!(isset($info['children'], $info['enabled'], $info['childselcount']) && $info['enabled'] && $info['childselcount'] > 0)) {
      return drush_set_error(t("No children for parent '@parent'", array("@parent" => $parent)));
    }
    foreach ($info['children'] as $childlabel => $childs) {
      if (count($childs) > 1) {
        // This should never happen...
        return drush_set_error(t("Found multiple children for child identifier '@childlabel'", array("@childlabel" => $childlabel)));
      }
      if (count($childs) == 0) {
        return drush_set_error(t("Found no children for child identifier '@childlabel'", array("@childlabel" => $childlabel)));
      }
      foreach ($childs as $cid => $enabled) {
        if ($childlabel !== $cid) {
          return drush_set_error(t("Not a PID: '@childlabel'", array("@childlabel" => $childlabel)));
        }
      }
    }
  }
  
  return TRUE;
} 

/**
 * Implements drush_hook_COMMAND().
 */
function drush_ubl_batch_compound_generate() {
  module_load_include('inc', 'ubl_batch_compound', 'includes/utilities');

  $csvfile = drush_get_option('csvfile');

  $result = '';
  
  $parent2child = ubl_batch_compound_read_csv($csvfile, 'drush_log', TRUE);

  $batch = ubl_batch_compound_make_batch($parent2child);
  $batch['progressive'] = FALSE;
  batch_set($batch);

  drush_backend_batch_process();
}
