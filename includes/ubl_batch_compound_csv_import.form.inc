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
 * The form for uploading csv.
 *
 * @param array $form
 *   The Drupal form definition.
 * @param array $form_state
 *   The Drupal form state.
 */
function ubl_batch_compound_import_form(array $form, array &$form_state) {
  $extensions = array('txt csv');
  $upload_size = min((int) ini_get('post_max_size'), (int) ini_get('upload_max_filesize'));
  $form['file'] = array(
    '#title' => t('Upload csv'),
    '#type' => 'managed_file',
    '#description' => t('Select a file to upload.<br/>Files must be less than <b>@size MB.</b><br/>Allowed file types: <b>@ext.</b>', array('@size' => $upload_size, '@ext' => $extensions[0])),
    '#upload_location' => 'temporary://',
    '#required' => TRUE,
    '#upload_validators' => array(
      'file_validate_extensions' => $extensions,
      'file_validate_size' => array($upload_size * 1024 * 1024),
    ),
  );
  $form['use_solr_field'] = array(
    '#type' => 'textfield',
    '#title' => t('Optional, use Solr field for search'),
    '#description' => t('Specify the Solr field that should be used to search the compound and children by. For example use "mods_identifier_local_s" to only search by identifier local.'),
    '#size' => 80,
    '#autocomplete_path' => 'islandora_solr/autocomplete_luke',
    '#default_value' => '',
  );
  $form['exact_match'] = array(
    '#type' => 'checkbox',
    '#title' => t('exact matches only'),
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Preview compounds'),
  );
  return $form;
}

/**
 * Processes uploaded csv.
 *
 * @param array $form
 *   The Drupal form definition.
 * @param array $form_state
 *   The Drupal form state.
 */
function ubl_batch_compound_import_form_submit(array $form, array &$form_state) {
  module_load_include('inc', 'ubl_batch_compound', 'includes/utilities');

  $file = file_load($form_state['values']['file']);
  $solrfield = $form_state['values']['use_solr_field'];
  $exactmatch = $form_state['values']['exact_match'];

  $path = drupal_realpath($file->uri);
  $parent2child = ubl_batch_compound_read_csv($path, 'drupal_set_message', FALSE, $solrfield, $exactmatch);

  $_SESSION['ubl_batch_compound_parent2child'] = $parent2child;
  $_SESSION['ubl_batch_compound_finished'] = FALSE;

  $form_state['redirect'] = 'ubl_batch_compound/confirm';
}

/**
 * The form for confirming the compound items.
 */
function ubl_batch_compound_import_confirm_form(array $form, array &$form_state) {
  $parent2child = $_SESSION['ubl_batch_compound_parent2child'];
  $options = array();
  $defaultvalues = array();
  if (count($form_state["build_info"]["args"]) == 1) {
    $parentid = $form_state["build_info"]["args"][0];

    drupal_set_title(t("Generate compound: viewing parent '@parentid'", array("@parentid" => $parentid)));

    foreach ($parent2child[$parentid]["children"] as $cid => $foundchildrenids) {
      if (count($foundchildrenids) == 0) {
        $rowid = "$parentid//$cid//none";
        $options[$rowid]['childid'] = check_plain($cid);
        $options[$rowid]['foundchildren'] = t('No child found');
        $options[$rowid]['#disabled'] = TRUE;
      }
      else {
        foreach ($foundchildrenids as $foundchildid => $isenabled) {
          $rowid = $parentid . '//' . $cid . '//' . $foundchildid;
          $options[$rowid]['childid'] = check_plain($cid);
          $options[$rowid]['foundchildren'] = array(
            'data' => array(
              '#type' => 'link',
              '#title' => filter_xss($foundchildid),
              '#href' => "islandora/object/{$foundchildid}",
            ),
          );
          $defaultvalues[$rowid] = $isenabled;
        }
      }
    }
    $table = array(
      '#type' => 'tableselect',
      '#header' => array(
        'childid' => t('Child ID in CSV'),
        'foundchildren' => t('Found Children'),
      ),
      '#options' => $options,
      '#default_value' => $defaultvalues,
      '#empty' => t('No data available.'),
      '#disabled' => ($_SESSION['ubl_batch_compound_finished']),
    );

    $form['parentid'] = array(
      '#type' => 'hidden',
      '#value' => $parentid,
    );

    if (!$_SESSION['ubl_batch_compound_finished']) {
      if (array_key_exists('singleparentid', $parent2child[$parentid])) {
        $form['usesingleparent'] = array(
          '#type' => 'checkbox',
          '#title' => t("Use existing object @parentid as parent", array("@parentid" => $parent2child[$parentid]['singleparentid'])),
          '#default_value' => $parent2child[$parentid]['usesingleparent'],
          '#return_value' => $parent2child[$parentid]['singleparentid'],
        );
      }
    }

    $form['child_table'] = $table;

    $submittitle = t('Save and back');
    if ($_SESSION['ubl_batch_compound_finished']) {
      $submittitle = t('Back');
    }
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => $submittitle,
    );
  }
  else {
    foreach ($parent2child as $pid => $values) {
      $options[$pid]['parentid'] = check_plain($pid);
      if (array_key_exists('singleparentid', $values) && (array_key_exists('usesingleparent', $values) && $values['usesingleparent'])) {
        $foundparent = $values["singleparentid"];
        $options[$pid]['parentitem'] = array(
          'data' => array(
            '#type' => 'link',
            '#title' => filter_xss($foundparent),
            '#href' => "islandora/object/{$foundparent}",
          ),
        );
      }
      else {
        if ($_SESSION['ubl_batch_compound_finished']) {
          $options[$pid]['parentitem'] = t('No parent made');
        }
        else {
          $options[$pid]['parentitem'] = t('New parent');
        }
      }
      $options[$pid]['childids'] = format_plural($values['childidcount'], "1 child in CSV", "@count children in CSV");
      if ($values['childfoundcount'] !== $values['childselcount']) {
        $options[$pid]['foundchildren'] = t("@selcount selected of @foundcount found", array("@selcount" => $values['childselcount'], "@foundcount" => $values['childfoundcount']));
      }
      else {
        $options[$pid]['foundchildren'] = format_plural($values['childfoundcount'], "1 child found", "@count children found");
      }
      $options[$pid]['view'] = array(
        'data' => array(
          '#type' => 'link',
          '#title' => t("View"),
          '#href' => "ubl_batch_compound/confirm/{$pid}",
          '#attributes' => array('class' => array('viewlinkclass')),
        ),
      );

      if ($values['childfoundcount'] == 0) {
        $defaultvalues[$pid] = FALSE;
        $options[$pid]['#disabled'] = TRUE;
      }
      else {
        $defaultvalues[$pid] = $values["enabled"];
      }
    }
    $table = array(
      '#type' => 'tableselect',
      '#header' => array(
        'parentid' => t('Parent ID in CSV'),
        'parentitem' => t('Parent item'),
        'childids' => t('Child IDs in CSV'),
        'foundchildren' => t('Found Children'),
        'view' => '',
      ),
      '#options' => $options,
      '#default_value' => $defaultvalues,
      '#empty' => t('No data available.'),
      '#disabled' => ($_SESSION['ubl_batch_compound_finished']),
    );

    if (!$_SESSION['ubl_batch_compound_finished']) {
      drupal_add_js(drupal_get_path('module', 'ubl_batch_compound') . '/js/ubl_batch_compound.js');
    }

    $form['redirecturl'] = array(
      '#type' => 'hidden',
      '#default_value' => 'xxxxx',
    );
    $form['parent_table'] = $table;

    if (!$_SESSION['ubl_batch_compound_finished']) {
      $form['submit'] = array(
        '#type' => 'submit',
        '#value' => t('Generate compound objects'),
      );
    }
  }

  return $form;
}

/**
 * Submitting the form for confirming the compound items.
 */
function ubl_batch_compound_import_confirm_form_submit(array $form, array &$form_state) {

  if (array_key_exists("redirecturl", $form_state["values"]) && $form_state["values"]["redirecturl"] !== 'xxxxx') {
    if (array_key_exists("parent_table", $form_state["values"])) {
      foreach ($form_state["values"]["parent_table"] as $parentid => $isneeded) {
        $_SESSION['ubl_batch_compound_parent2child'][$parentid]['enabled'] = $isneeded;
      }
    }
    $form_state['redirect'] = $form_state["values"]["redirecturl"];
    return;
  }
  if ($_SESSION['ubl_batch_compound_finished']) {
    // Do not change anything if already finished.
    if (array_key_exists("child_table", $form_state["values"])) {
      $form_state['redirect'] = 'ubl_batch_compound/confirm';
    }
    return;
  }

  if (array_key_exists("parent_table", $form_state["values"])) {
    module_load_include('inc', 'ubl_batch_compound', 'includes/utilities');

    foreach ($form_state["values"]["parent_table"] as $parentid => $isneeded) {
      $_SESSION['ubl_batch_compound_parent2child'][$parentid]['enabled'] = $isneeded;
    }
    $batch = ubl_batch_compound_make_batch($_SESSION['ubl_batch_compound_parent2child']);
    if (isset($batch)) {
      batch_set($batch);
    }
  }
  else {
    $parent2child = $_SESSION['ubl_batch_compound_parent2child'];
    $parentid = $form_state["values"]["parentid"];

    if (array_key_exists("usesingleparent", $form_state["values"])) {
      $parent2child[$parentid]["usesingleparent"] = ($form_state["values"]["usesingleparent"] !== 0);
    }
    foreach ($form_state["values"]["child_table"] as $childpath => $isneeded) {
      $parts = explode("//", $childpath);
      if (count($parts) == 3) {
        $parentid = $parts[0];
        $childcvsid = $parts[1];
        $childid = $parts[2];
        $parent2child[$parentid]["children"][$childcvsid][$childid] = ($isneeded !== 0);
      }
    }
    $parent2child[$parentid]["childselcount"] = 0;
    foreach ($parent2child[$parentid]["children"] as $childs) {
      foreach ($childs as $cid => $enabled) {
        if ($enabled) {
          $parent2child[$parentid]["childselcount"]++;
        }
      }
    }

    $_SESSION['ubl_batch_compound_parent2child'] = $parent2child;

    $form_state['redirect'] = 'ubl_batch_compound/confirm';
  }
}
