<?php

/**
 * @file
 * Handles the compound item creation from csv import.
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
 * Reads csv file.
 *
 * @param array $form
 *   The Drupal form definition.
 * @param array $form_state
 *   The Drupal form state.
 */
function ubl_batch_compound_read_csv($csvfilepath, $logfunction, $onlypids = FALSE, $solrfield = NULL, $exactmatch = FALSE) {
  ini_set("auto_detect_line_endings", "1");
  $delimit = "\t";
  $parent2child = array();
  $child2parent = array();
  $linenr = 0;
  $rels_predicate = variable_get('islandora_compound_object_relationship', 'isConstituentOf');
  if (($fh = fopen($csvfilepath, "r")) !== FALSE) {
    while (($data = fgetcsv($fh, 0, $delimit)) !== FALSE) {
      $linenr++;
      if ($linenr == 1) {
        // Treat header special.
        if (count($data) == 1) {
          $delimit = ",";
          $data = str_getcsv($data[0], $delimit);
        }
        if (count($data) == 1) {
          $delimit = ";";
          $data = str_getcsv($data[0], $delimit);
        }
        $parentid = $data[0];
        $childid = $data[1];

        $foundparents = find_fedora_object_ids($parentid, $onlypids, $exactmatch, $solrfield);
        $foundchildren = find_fedora_object_ids($childid, $onlypids, $exactmatch, $solrfield);

        if (count($foundparents) != 1 && count($foundchildren) == 0) {
          // This is a header, so skip it.
          continue;
        }
      }
      if (count($data) < 2) {
        $logfunction(t("Unexpected line at @linenr: @line", array("@linenr" => $linenr, "@line" => implode($delimit, $data))), 'warning');
        continue;
      }

      $parentid = $data[0];
      $childid = $data[1];

      $foundparents = find_fedora_object_ids($parentid, $onlypids, $exactmatch, $solrfield);
      $foundchildren = find_fedora_object_ids($childid, $onlypids, $exactmatch, $solrfield);

      if (count($foundchildren) == 0) {
        $logfunction(t("Found no children for id '@childid'", array("@childid" => $childid)), 'warning');
      }

      if (!array_key_exists($parentid, $parent2child)) {
        $parent2child[$parentid] = array(
          "childidcount" => 0,
          "childfoundcount" => 0,
          "childselcount" => 0,
          "parent" => $foundparents,
          "enabled" => TRUE,
          "children" => array(),
          "childrentypeinfo" => array(),
        );
        if (count($foundparents) == 1) {
          $foundparentid = $foundparents[0];
          $foundparent = islandora_object_load($foundparentid);
          $compound = FALSE;
          if ($foundparent) {
            if (variable_get('islandora_compound_object_compound_children', TRUE)) {
              // Has setting that only compounds are allowed to have children.
              foreach ($foundparent->models as $value) {
                if ($value == "islandora:compoundCModel") {
                  $compound = TRUE;
                }
              }
            }
            else {
              // Has setting that all objects may have child objects.
              $compound = TRUE;
            }
            if ($compound) {
              $parent2child[$parentid]["singleparentid"] = $foundparentid;
              $parent2child[$parentid]["usesingleparent"] = TRUE;
            }
            else {
              $logfunction(t("Found for parent with id '@parentid' a Islandora object (@objectid) that is not an compound object", array("@parentid" => $parentid, "@objectid" => $foundparentid)), 'warning');
            }
          }
        }
      }
      $parent2child[$parentid]["childidcount"]++;
      if (!array_key_exists($childid, $parent2child[$parentid]["children"])) {
        $parent2child[$parentid]["children"][$childid] = array_fill_keys($foundchildren, TRUE);
        $parent2child[$parentid]["childfoundcount"] += count($foundchildren);
        $parent2child[$parentid]["childselcount"] += count($foundchildren);
        if (count($data) >= 4) {
          $parent2child[$parentid]["childrentypeinfo"][$childid] = array("type" => $data[2], "info" => $data[3]);
        }

        if (array_key_exists("singleparentid", $parent2child[$parentid])
           && (array_key_exists("usesingleparent", $parent2child[$parentid]) && $parent2child[$parentid]["usesingleparent"])) {
          $foundparentid = $parent2child[$parentid]["singleparentid"];
          foreach ($foundchildren as $foundchild) {
            if ($foundchild === $foundparentid) {
              $logfunction(t("Child (@childid) with itself as parent (@parentid) is not allowed",
                array(
                  "@childid" => $childid,
                  "@parentid" => $parentid,
                )), 'error');
              $parent2child[$parentid]["usesingleparent"] = FALSE;
            }
          }
        }
        foreach ($foundchildren as $foundchild) {
          if (array_key_exists($foundchild, $child2parent)) {
            $logfunction(t("Child (@foundchild, found for '@childid') already belongs to parent (@parentid) so will not be added to parent (@otherparentid)",
              array(
                "@foundchild" => $foundchild,
                "@childid" => $childid,
                "@parentid" => $child2parent[$foundchild],
                "@otherparentid" => $parentid,
              )), 'warning');
            $parent2child[$parentid]["children"][$childid][$foundchild] = FALSE;
            $parent2child[$parentid]["childselcount"]--;
          }
          else {
            $child2parent[$foundchild] = $parentid;
          }
          $childobject = islandora_object_load($foundchild);
          if ($childobject) {
            $part_of = $childobject->relationships->get(FEDORA_RELS_EXT_URI, $rels_predicate);
            // This object is part of a compound object.
            if (!empty($part_of) && count($part_of) > 0 && $part_of[0]['object']['value'] !== $foundparentid) {
              $logfunction(t("Child (@foundchild, found for '@childid') already has a parent (@parentid)",
                array(
                  "@foundchild" => $foundchild,
                  "@childid" => $childid,
                  "@parentid" => $part_of[0]['object']['value'],
                )), 'warning');
              if ($parent2child[$parentid]["children"][$childid][$foundchild]) {
                $parent2child[$parentid]["children"][$childid][$foundchild] = FALSE;
                $parent2child[$parentid]["childselcount"]--;
              }
            }
          }
        }
      }
    }
    fclose($fh);

    foreach ($parent2child as $parendid => $childreninfo) {
      $currenttype = NULL;
      if (count($childreninfo["childrentypeinfo"]) > 0) {
        foreach ($childreninfo["childrentypeinfo"] as $childid => $typeinfo) {
          if ($currenttype === NULL) {
            $currenttype = $typeinfo["type"];
          }
          elseif ($currenttype !== $typeinfo["type"]) {
            $logfunction(t("Child (@childid) has other type (@childtype) than other children (@firstchildtype) of parent @parentid.",
              array(
                "@childid" => $childid,
                "@parentid" => $parentid,
                "@childtype" => $typeinfo["type"],
                "@firstchildtype" => $currenttype,
              )), 'error');
          }
        }
      }
    }
  }
  return $parent2child;
}

/**
 * Makes a batch with batch operations.
*/
function ubl_batch_compound_make_batch(array $parent2child) {

  $operations = array();

  foreach ($parent2child as $parentid => $oneparent2children) {
    if ($oneparent2children['enabled']) {
      $operations[] = array('ubl_batch_compound_batch_set_children', array($oneparent2children, $parentid));
    }
  }
  if (count($operations) > 0) {
    $batch = array(
      'operations' => $operations,
      'title' => t("Generating compound objects"),
      'init_message' => t("Preparing ..."),
      'progress_message' => t('Time elapsed: @elapsed <br/>Estimated time remaining @estimate.'),
      'error_message' => t('An error has occurred.'),
      'file' => drupal_get_path('module', 'ubl_batch_compound') . '/includes/utilities.inc',
      'finished' => 'ubl_batch_compound_batch_set_children_finished',
    );
    return $batch;
  }
}

/**
 * Make the compound for one parentid and the children that were selected.
 */
function ubl_batch_compound_batch_set_children($oneparent2children, $parentid, &$context) {
  module_load_include('inc', 'islandora_compound_object', 'includes/compound_types');
  module_load_include('inc', 'islandora_compound_object', 'includes/manage.form');
  module_load_include('inc', 'islandora_basic_collection', 'includes/utilities');
  module_load_include('inc', 'islandora');

  $parent = FALSE;
  if (array_key_exists('singleparentid', $oneparent2children) && (array_key_exists('usesingleparent', $oneparent2children) && $oneparent2children['usesingleparent'])) {
    $parent = islandora_object_load($oneparent2children['singleparentid']);
  }
  $childids = array();
  foreach ($oneparent2children["children"] as $childs) {
    foreach ($childs as $cid => $enabled) {
      if ($enabled) {
        $childids[] = $cid;
      }
    }
  }
  if (count($childids) > 0) {
    $firstchildid = $childids[0];
  }
  $needsparentcheck = ($parent !== FALSE);
  if ($parent === FALSE) {
    if (isset($firstchildid)) {
      $firstchild = islandora_object_load($firstchildid);
      if ($firstchild) {
        $collection = islandora_object_load('islandora:compound_collection');
        $tuque = islandora_get_tuque_connection();
        $parent = $tuque->repository->constructObject(islandora_get_namespace($firstchild->id));
        $parent->label = $parentid;
        $parent->models = 'islandora:compoundCModel';
        islandora_basic_collection_add_to_collection($parent, $collection);
        $tuque->repository->ingestObject($parent);
      }
    }
    if ($parent === FALSE) {
      return;
    }
    $_SESSION['ubl_batch_compound_parent2child'][$parentid]['singleparentid'] = $parent->id;
    $_SESSION['ubl_batch_compound_parent2child'][$parentid]['usesingleparent'] = TRUE;
  }

  $context['message'] = t('Generating compound object @parentid', array('@parentid' => $parent->id));

  if (isset($oneparent2children["childrentypeinfo"][$firstchildid]["type"]) && function_exists("islandora_compound_object_set_type")) {
    $type = $oneparent2children["childrentypeinfo"][$firstchildid]["type"];

    $shouldchangeparenttype = TRUE;
    if ($needsparentcheck) {
      $currenttype = islandora_compound_object_get_type($parent);
      if ($currenttype === $type) {
        $shouldchangeparenttype = FALSE;
      }
    }
    if ($shouldchangeparenttype) {
      islandora_compound_object_set_type($parent, $type);
    }
  }

  $objecttypes = islandora_compound_object_types();
  $type = "";
  $rels_predicate = variable_get('islandora_compound_object_relationship', 'isConstituentOf');
  $insert_seq = 0;
  $escaped_parent_id = str_replace(':', '_', $parent->id);
  $compounds = array();
  foreach ($childids as $childid) {
    $child_object = islandora_object_load($childid);
    if ($child_object) {
      $insert_seq++;

      $compounds[$childid] = array("weight" => $insert_seq, "object" => $child_object);
      if (isset($oneparent2children["childrentypeinfo"][$childid]["type"]) && function_exists("islandora_compound_object_update_relation")) {
        $type = $oneparent2children["childrentypeinfo"][$childid]["type"];
        if (isset($objecttypes[$type]['relations']['predicate'])) {
          $predicate = $objecttypes[$type]['relations']['predicate'];
          $relvalue = $oneparent2children["childrentypeinfo"][$childid]["info"];
          $compounds[$childid]["relation"] = $relvalue;
          $compounds[$childid]["predicate"] = $predicate;
        }
      }
    }
  }

  if (isset($objecttypes[$type]['comparefunction'])) {
    $comparefunc = $objecttypes[$type]['comparefunction'];
    uasort($compounds, $comparefunc);
  }

  $insert_seq = 0;
  foreach ($compounds as $childid => $info) {
    $child_object = $info["object"];

    $shouldcommit = FALSE;
    $child_object->relationships->autoCommit = FALSE;
    $fedorarels = $child_object->relationships->get(FEDORA_RELS_EXT_URI, $rels_predicate);
    if (count($fedorarels) == 0 || (isset($fedorarels[0]['object']['value']) && $fedorarels[0]['object']['value'] != $parent->id)) {
      $child_object->relationships->remove(FEDORA_RELS_EXT_URI, $rels_predicate);
      $child_object->relationships->add(FEDORA_RELS_EXT_URI, $rels_predicate, $parent->id);
      $shouldcommit = TRUE;
    }
    $islandorarels = $child_object->relationships->get(ISLANDORA_RELS_EXT_URI, "isSequenceNumberOf$escaped_parent_id");
    if (count($islandorarels) == 0 || (isset($islandorarels[0]['object']['value']) && $islandorarels[0]['object']['value'] != $insert_seq)) {
      $child_object->relationships->remove(ISLANDORA_RELS_EXT_URI, "isSequenceNumberOf$escaped_parent_id");
      $child_object->relationships->add(ISLANDORA_RELS_EXT_URI, "isSequenceNumberOf$escaped_parent_id", $insert_seq, RELS_TYPE_PLAIN_LITERAL);
      $shouldcommit = TRUE;
    }
    if (isset($info["predicate"], $info["relation"]) && function_exists("islandora_compound_object_update_relation")) {
      $relation = $info["predicate"];
      $relvalue = $info["relation"];
      $hascommitted = islandora_compound_object_update_relation($parent->id, $childid, $relation, $relvalue, $context);
      if ($shouldcommit) {
        $shouldcommit = !$hascommitted;
      }
    }
    if ($shouldcommit) {
      $child_object->relationships->commitRelationships();
    }
  }
  islandora_compound_object_update_parent_thumbnail($parent);
}

/**
 * Finish making the compounds.
 */
function ubl_batch_compound_batch_set_children_finished() {
  $_SESSION['ubl_batch_compound_finished'] = TRUE;
}

/* helper functions */

/**
 * Finding the Fedora object IDs of objects by identifier.
 */
function find_fedora_object_ids($identifier, $isvalidpid = FALSE, $exactmatch = FALSE, $query_field = FALSE) {
  module_load_include('inc', 'islandora', 'includes/utilities');
  module_load_include('inc', 'islandora_solr_search', 'includes/query_processor');
  module_load_include('inc', 'islandora_solr_search', 'includes/utilities');

  static $alreadyfound = array();

  if (islandora_is_valid_pid($identifier)) {
    // This looks like a islandora object id.
    if (array_key_exists($identifier, $alreadyfound)) {
      return $alreadyfound[$identifier];
    }

    $obj = islandora_object_load($identifier);

    if ($obj) {
      $alreadyfound[$identifier] = array($identifier);

      return $alreadyfound[$identifier];
    }
  }
  if ($isvalidpid) {
    // apparently the given identifier is not a valid pid but we are told that it was valid...
    return array();
  }
  static $qp = NULL;
  static $qpc = 0;
  if (is_null($qp) || $qpc > 10) {
    // Get a fresh copy the first time and after each 10 queries.
    // Apparently there are limitations on the amount of queries it can handle.
    $qp = new IslandoraSolrQueryProcessor();
    $qpc = 0;
  }
  $qpc++;

  $escapedid = islandora_solr_lesser_escape($identifier);
  if ($query_field) {
    $search = "$query_field:$escapedid";
  }
  else {
    $search = "catch_all_fields_mt:$escapedid";
  }

  if (array_key_exists($search, $alreadyfound)) {
    return $alreadyfound[$search];
  }

  $qp->buildAndExecuteQuery($search, NULL, FALSE);

  $r = $qp->islandoraSolrResult;
  $numfound = $r['response']['numFound'];
  $objectcount = count($r['response']['objects']);

  $objectids = array();
  for ($i = 0; $i < $numfound && $i < $objectcount; $i++) {
    $objid = $r['response']['objects'][$i]['PID'];

    if ($exactmatch) {
      $hasexactmatch = FALSE;
      $solrdoc = $r['response']['objects'][$i]['solr_doc'];
      if ($query_field) {
        if ($solrdoc[$query_field] === $identifier) {
          $hasexactmatch = TRUE;
        }
      }
      else {
        foreach ($solrdoc as $solrkey => $solrval) {
          if (is_array($solrval)) {
            foreach ($solrval as $sv) {
              if ($sv === $identifier) {
                $hasexactmatch = TRUE;
                break 2;
              }
            }
          }
          else {
            if ($solrval === $identifier) {
              $hasexactmatch = TRUE;
              break;
            }
          }
        }
      }
      if (!$hasexactmatch) {
        continue;
      }
    }

    if ($objid) {
      $objectids[] = $objid;
    }
  }

  $alreadyfound[$search] = $objectids;

  return $objectids;
}
