
/* Ekiga -- A VoIP and Video-Conferencing application
 * Copyright (C) 2000-2006 Damien Sandras
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *
 * Ekiga is licensed under the GPL license and as a special exception,
 * you have permission to link or otherwise combine this program with the
 * programs OPAL, OpenH323 and PWLIB, and distribute the combination,
 * without applying the requirements of the GNU GPL to the OPAL, OpenH323
 * and PWLIB programs, as long as you do follow the requirements of the
 * GNU GPL for all the rest of the software thus combined.
 */


/*
 *                         gmcontacts-gmconf.cpp - description 
 *                         ---------------------------
 *   begin                : Mon Apr 19 2004
 *   copyright            : (C) 2004-2006 by Julien Puydt
 *                          (C) 2004-2006 by Damien Sandras
 *                          (C) 2006 by Kilian Krause
 *   description          : Implementation of an addressbook using gmconf
 *
 */


#include "../../config.h"

#include "gmconf.h"
#include "gmcontacts.h"
#ifndef _GM_CONTACTS_H_INSIDE__
#define _GM_CONTACTS_H_INSIDE__
#include "gmcontacts-local.h"
#include "gmcontacts-convert.h"
#undef _GM_CONTACTS_H_INSIDE__
#endif
#include <stdlib.h>
#include <string.h>

/*
 * The following is the implementation of the local addressbooks, when
 * gnomemeeting is compiled without evolution-data-server support: it uses
 * the configuration system (gmconf) to store the data.
 *
 * Everything is stored under CONTACTS_KEY, in the following form:
 * - an addressbook identifier is an integer starting at 1 ;
 * (stored as a string) ;
 * - the integer key CONTACTS_KEY "max_aid" stores the maximum addressbook
 * identifier (to know when to stop when searching the list of addressbooks:
 * there may be an empty slot in the middle after a removal, so looking for
 * empty slot isn't a good idea) ;
 * - the addressbook is stored in the CONTACTS_KEY (string of the aid)
 * namespace ;
 * - the addressbook features are directly stored in the namespace: for example
 * CONTACTS_KEY (string of the aid)"/name" stores the name of the
 * group ;
 * - a contact identifier is an integer starting at 1 (and has a meaning in the
 * addressbook only) ;
 * - a contact is stored as a vcard in the addressbook namespace:
 * CONTACTS_KEY (string of the aid) "/" (string of the uid) ;
 * - the integer key CONTACTS_KEY (string of the aid)"/max_uid" stores the
 * maximum contact identifier (for the same reasons as above).
 *
 * NB: starting the counts at 1 has the bonus that a 0 can mean "doesn't exist"
 */

#define CONTACTS_KEY "/apps/" PACKAGE_NAME "/contacts/"


/*
 * Declaration of the helper functions
 */

/* this function checks if str2 is a substring of str2
 */
static gboolean str_contains (const gchar *str1, const gchar *str2);

/* this function retrieves the contact with the given "coordinates"
 * from the configuration ; it wants and checks aid > 0 and uid > 0.
 */
static GmContact *get_contact (gint aid, 
			       gint uid);


/* this function stores the given contact in the configuration, at the
 * given "coordinates" ; it wants and checks aid > 0, and contact != NULL
 * with a valid (> 0) uid.
 */
static gboolean store_contact (GmContact *contact, 
			       gint aid); 


/* this function retrieves the addressbook with the given identifier
 * from the configuration ; it wants and checks aid > 0.
 */
static GmAddressbook *get_addressbook (gint aid);


/* this function stores the addressbook in the configuration, at the
 * given identifier ; it wants and checks addb != NULL with a valid aid.
 */
static gboolean store_addressbook (GmAddressbook *addb);


/* this function returns the first available addressbook identifier
 * (that can be max_aid+1 or the old identifier of a removed group)
 */
static gint get_available_aid ();


/* this function returns the first available contact identifier
 * available in the given group (again, that can be either max_uid+1 or
 * the old identifier of a removed contact) [it wants and checks aid > 0]
 */
static gint get_available_uid (gint aid);


/*
 * Implementation of the helper functions
 */


static
gboolean str_contains (const gchar *str1, const gchar *str2)
{
  return (g_strrstr (str1, str2) != NULL);
}


static GmContact *
get_contact (gint aid, 
	     gint uid)
{
  gchar *vcard = NULL;
  gchar *key = NULL;
  GmContact *contact = NULL;

  g_return_val_if_fail (aid > 0, NULL);
  g_return_val_if_fail (uid > 0, NULL);
  
  key = g_strdup_printf (CONTACTS_KEY "%d/%d", aid, uid);
  vcard = gm_conf_get_string (key);
  g_free (key);

  if (vcard != NULL) {
    contact = vcard_to_gmcontact (vcard);
    contact->uid = g_strdup_printf ("%d", uid);
  }

  return contact;
}


static gboolean
store_contact (GmContact *contact, 
	       gint aid)
{
  gchar *vcard = NULL;
  gchar *key = NULL;
  gint uid = 0;
  gint max_uid = 0;

  g_return_val_if_fail (contact != NULL, FALSE);
  g_return_val_if_fail (contact->uid != NULL, FALSE);
  g_return_val_if_fail (aid > 0, FALSE);

  uid = strtol (contact->uid, NULL, 10);
  g_return_val_if_fail (uid > 0, FALSE);

  vcard = gmcontact_to_vcard (contact);
  key = g_strdup_printf (CONTACTS_KEY "%d/%d", aid, uid);
  gm_conf_set_string (key, vcard);
  g_free (key);  
  g_free (vcard);

  key = g_strdup_printf (CONTACTS_KEY "%d/max_uid", aid);
  max_uid = gm_conf_get_int (key);
  if (uid > max_uid)
    gm_conf_set_int (key, uid);
  g_free (key);

  return TRUE;
}


static GmAddressbook *
get_addressbook (gint aid)
{
  GmAddressbook *addb = NULL;
  gchar *key = NULL;
  gchar *str = NULL;

  g_return_val_if_fail (aid > 0, NULL);

  addb = gm_addressbook_new ();

  addb->aid = g_strdup_printf ("%d", aid);

  key = g_strdup_printf (CONTACTS_KEY "%d/name", aid);
  str = gm_conf_get_string (key);
  if (str != NULL)
    addb->name = g_strdup (str);
  g_free (key);

  key = g_strdup_printf (CONTACTS_KEY "%d/url", aid);
  str = gm_conf_get_string (key);
  if (str != NULL)
    addb->url = g_strdup (str);
  g_free (key);

  key = g_strdup_printf (CONTACTS_KEY "%d/call_attribute", aid);
  str = gm_conf_get_string (key);
  if (str != NULL)
    addb->call_attribute = g_strdup (str);
  g_free (key);

  if (addb->name == NULL
      && addb->url == NULL 
      && addb->call_attribute == NULL) {
    gm_addressbook_delete (addb);
    addb = NULL;
  }

  return addb;
}


static gboolean
store_addressbook (GmAddressbook *addb)
{
  gchar *key = NULL;
  gint max_aid = 0;
  gint aid = 0;

  g_return_val_if_fail (addb != NULL, FALSE);
  g_return_val_if_fail (addb->aid != NULL, FALSE);

  aid = strtol (addb->aid, NULL, 10);
  
  g_return_val_if_fail (aid > 0, FALSE);

  key = g_strdup_printf (CONTACTS_KEY "%d/name", aid);
  if (addb->name != NULL)
    gm_conf_set_string (key, addb->name);
  else
    gm_conf_destroy (key);
  g_free (key);

  key = g_strdup_printf (CONTACTS_KEY "%d/url", aid);
  if (addb->url != NULL)
    gm_conf_set_string (key, addb->url);
  else
    gm_conf_destroy (key);
  g_free (key);

  key = g_strdup_printf (CONTACTS_KEY "%d/call_attribute", aid);
  if (addb->call_attribute != NULL)
    gm_conf_set_string (key, addb->call_attribute);
  else
    gm_conf_destroy (key);
  g_free (key);

  max_aid = gm_conf_get_int (CONTACTS_KEY "max_aid");
  if (aid > max_aid)
    gm_conf_set_int (CONTACTS_KEY "max_aid", aid);

  return TRUE;
}


static gint
get_available_aid ()
{
  gint aid = 1;
  gint max_aid = 1;
  GmAddressbook *addb = NULL;

  max_aid = gm_conf_get_int (CONTACTS_KEY "max_aid");
  
  for (aid = 1; aid < max_aid + 1; aid++) {
    addb = get_addressbook (aid);
    if (addb == NULL)
      break;
    gm_addressbook_delete (addb);
  }
  
  return aid;
}


static gint
get_available_uid (gint aid)
{
  gint uid = 1;
  gint max_uid = 1;
  gchar *key = NULL;
  GmContact *contact = NULL;

  g_return_val_if_fail (aid > 0, 1);

  key = g_strdup_printf (CONTACTS_KEY "%d/max_uid", aid);
  max_uid = gm_conf_get_int (key);
  g_free (key);
  
  for (uid = 1; uid < max_uid + 1; uid++) {
    contact = get_contact (aid, uid);
    if (contact == NULL)
      break;
    gmcontact_delete (contact);
  }

  return uid;
}


/*
 * Implementation of the public api
 */


GmContact *
gmcontact_new ()
{
  GmContact *contact = NULL;

  contact = g_new (GmContact, 1);

  contact->fullname = NULL;
  contact->categories = NULL;
  contact->url = NULL;
  contact->location = NULL;
  contact->speeddial = NULL;
  contact->comment = NULL;
  contact->software = NULL;
  contact->email = NULL;
  contact->state = CONTACT_AVAILABLE;
  contact->video_capable = FALSE;
  contact->uid = NULL; 

  return contact;
}


GmContact *
gmcontact_copy (GmContact *orig)
{
  GmContact *contact = NULL;

  contact = g_new (GmContact, 1);

  contact->fullname = g_strdup (orig->fullname);
  contact->categories = g_strdup (orig->categories);
  contact->url = g_strdup (orig->url);
  contact->location = g_strdup (orig->location);
  contact->speeddial = g_strdup (orig->speeddial);
  contact->comment = g_strdup (orig->comment);
  contact->software = g_strdup (orig->software);
  contact->email = g_strdup (orig->email);
  contact->state = orig->state;
  contact->video_capable = orig->video_capable;
  contact->uid = g_strdup (orig->uid); 

  return contact;
}


void
gmcontact_delete (GmContact *contact)
{
  if (!contact)
    return;

  g_free (contact->fullname);
  g_free (contact->url);
  g_free (contact->speeddial);
  g_free (contact->categories);
  g_free (contact->comment);
  g_free (contact->software);
  g_free (contact->email);
  g_free (contact->uid);

  g_free (contact);
}


GmAddressbook *
gm_addressbook_new ()
{
  GmAddressbook *addressbook = NULL;

  addressbook = g_new (GmAddressbook, 1);
  
  addressbook->name = NULL;
  addressbook->url = NULL;
  addressbook->aid = g_strdup_printf ("%010d", g_random_int());
  addressbook->call_attribute = NULL;

  return addressbook;
}


void 
gm_addressbook_delete (GmAddressbook *addressbook)
{
  if (!addressbook)
    return;

  g_free (addressbook->url);
  g_free (addressbook->aid);
  g_free (addressbook->name);
  g_free (addressbook->call_attribute);

  g_free (addressbook);  
}


GSList *
gnomemeeting_get_local_addressbooks ()
{
  gint i = 0;
  gint max_aid = 0;
  GmAddressbook *addb = NULL;
  GSList *result = NULL;

  max_aid = gm_conf_get_int (CONTACTS_KEY "max_aid");

  for (i = 1; i <= max_aid ; i++) {
    addb = get_addressbook (i);
    if (addb != NULL)
      result = g_slist_append (result, (gpointer)addb);
  }

  return result;
}


GSList *
gnomemeeting_local_addressbook_get_contacts (GmAddressbook *addb,
					     int & nbr,
					     gboolean partial_match,
					     gchar *fullname,
					     gchar *url,
					     gchar *categorie,
					     gchar *location,
					     gchar *speeddial)
{
  gchar *key = NULL;
  gint aid = 0;
  gint uid = 0;
  gint max_uid = 0;
  gint max_aid = 0;
  gboolean matching = TRUE;
  GmContact *contact = NULL;
  GSList *result = NULL;
  GmAddressbook *addb_loop = NULL;

  if (addb != NULL) {
    
    aid = strtol (addb->aid, NULL, 10);
 
    g_return_val_if_fail (aid > 0, NULL);
   
    key = g_strdup_printf (CONTACTS_KEY "%d/max_uid", aid);
    max_uid = gm_conf_get_int (key);
    g_free (key);
    
    for (uid = 1; uid <= max_uid ; uid++) {
      contact = get_contact (aid, uid);
      if (contact != NULL) {
	
	if (partial_match)
	  matching = FALSE;
	else
	  matching = TRUE;

	if (fullname) {

	  if (str_contains (contact->fullname, fullname)) {

	    if (partial_match)
	      matching = TRUE;
	  } else
	    if (!partial_match)
	      matching = FALSE;
	}

	if (url) {

	  if (str_contains (contact->url, url)) {

	    if (partial_match)
	      matching = TRUE;
	  } else
	    if (!partial_match)
	      matching = FALSE;
	}

	if (categorie) {

	  if (str_contains (contact->categories, categorie)) {

	    if (partial_match)
	      matching = TRUE;
	  } else
	    if (!partial_match)
	      matching = FALSE;
	}

	if (speeddial) {
    
	  if (str_contains (contact->speeddial, speeddial)) {

	    if (partial_match)
	      matching = TRUE;
	  } else
	    if (!partial_match)
	      matching = FALSE;
	}

	if (partial_match
	    && fullname == NULL
	    && url == NULL
	    && categorie == NULL
	    && speeddial == NULL)
	  matching = TRUE; /* nothing was tested, so the contact is good */

	if (matching)
	  result = g_slist_append (result, (gpointer)contact);
	else
	  gmcontact_delete (contact);
      }
    }
  }
  else {
    max_aid = gm_conf_get_int (CONTACTS_KEY "max_aid");
    for (aid = 1; aid <= max_aid; aid++) {
      addb_loop = get_addressbook (aid);
      if (addb_loop != NULL)
	result = g_slist_concat (result, gnomemeeting_local_addressbook_get_contacts (addb_loop, nbr, partial_match, fullname, url, categorie, location, speeddial));
    }
  }

  nbr = g_slist_length (result);

  return result;
}


gboolean
gnomemeeting_local_addressbook_add (GmAddressbook *addb)
{
  gint aid = 0;
  
  g_return_val_if_fail (addb != NULL, FALSE);

  aid = get_available_aid ();

  if (addb->aid) {

    g_free (addb->aid);
  }
  addb->aid = g_strdup_printf ("%d", aid);

  return store_addressbook (addb);
}


gboolean
gnomemeeting_local_addressbook_delete (GmAddressbook *addb)
{
  gint aid = 0;
  gint max_aid = 0;
  gchar *namespc = NULL;

  g_return_val_if_fail (addb != NULL, FALSE);

  aid = strtol (addb->aid, NULL, 10);

  g_return_val_if_fail (aid > 0, FALSE);

  namespc = g_strdup_printf (CONTACTS_KEY "%d", aid);
  gm_conf_destroy (namespc);
  g_free (namespc);

  max_aid = gm_conf_get_int (CONTACTS_KEY "max_aid");

  if (max_aid == aid) {
    /* FIXME: bad! Need a proper loop to detect the last really
     * used aid (another helper function would be nice, I think)
     */
    gm_conf_set_int (CONTACTS_KEY "max_aid", aid - 1);
  }

  return TRUE;
}


gboolean
gnomemeeting_local_addressbook_modify (GmAddressbook *addb)
{
  g_return_val_if_fail (addb != NULL, FALSE);
 
  return store_addressbook (addb);
}


gboolean
gnomemeeting_local_addressbook_delete_contact (GmAddressbook *addb,
					       GmContact *contact)
{
  gint aid = 0;
  gint uid = 0;
  gint max_uid = 0;
  gchar *namespc = NULL;
  gchar *key = NULL;

  g_return_val_if_fail (addb != NULL, FALSE);
  g_return_val_if_fail (contact != NULL, FALSE);

  aid = strtol (addb->aid, NULL, 10);
  g_return_val_if_fail (aid > 0, FALSE);

  uid = strtol (contact->uid, NULL, 10);
  g_return_val_if_fail (uid > 0, FALSE);

  namespc = g_strdup_printf (CONTACTS_KEY "%d/%d", aid, uid);
  gm_conf_destroy (namespc);
  g_free (namespc);

  key = g_strdup_printf (CONTACTS_KEY "%d/max_uid", aid);
  max_uid = gm_conf_get_int (key);
  g_free (key);

  if (max_uid == uid) {
    /* FIXME: bad! Need a proper loop to detect the last really
     * used uid (another helper function would be nice, I think)
     */
    key = g_strdup_printf (CONTACTS_KEY "%d/max_uid", aid);
    gm_conf_set_int (key, uid - 1);
    g_free (key);
  }

  return TRUE;
}


gboolean
gnomemeeting_local_addressbook_add_contact (GmAddressbook *addb,
					    GmContact *contact)
{
  gint aid = 0;
  gint uid = 0;

  g_return_val_if_fail (addb != NULL, FALSE);
  g_return_val_if_fail (contact != NULL, FALSE);

  aid = strtol (addb->aid, NULL, 10);
  g_return_val_if_fail (aid > 0, FALSE);

  uid = get_available_uid (aid);
  contact->uid = g_strdup_printf ("%d", uid);

  return store_contact (contact, aid);
}


gboolean
gnomemeeting_local_addressbook_modify_contact (GmAddressbook *addb,
					       GmContact *contact)
{
  gint aid = 0;

  g_return_val_if_fail (addb != NULL, FALSE);
  g_return_val_if_fail (contact != NULL, FALSE);

  aid = strtol (addb->aid, NULL, 10);
  g_return_val_if_fail (aid > 0, FALSE);

  return store_contact (contact, aid);
}


gboolean 
gnomemeeting_local_addressbook_is_editable (GmAddressbook *)
{
  return TRUE;
}


void
gnomemeeting_local_addressbook_init (gchar *group_name,
				     gchar *source_name)
{
  GmAddressbook *addb = NULL;
  
  GSList *addressbooks = NULL;
  GSList *addressbooks_iter = NULL;
  gboolean found = false;

  addressbooks = gnomemeeting_get_local_addressbooks ();
  addressbooks_iter = addressbooks;
  while (addressbooks_iter && !found) {

    if (addressbooks_iter->data) {

      addb = GM_ADDRESSBOOK (addressbooks_iter->data);
      if (addb && addb->name && !strcmp (source_name, addb->name))
        found = true;
    }
    addressbooks_iter = g_slist_next (addressbooks_iter);
  }
  g_slist_foreach (addressbooks, (GFunc) gm_addressbook_delete, NULL);
  g_slist_free (addressbooks);

  if (found)
    return;

  addb = gm_addressbook_new ();
  addb->name = g_strdup (source_name);
  gnomemeeting_addressbook_add (addb);
  gm_addressbook_delete (addb);
}


gboolean 
gnomemeeting_local_addressbook_has_fullname (GmAddressbook *)
{
  return TRUE;
}


gboolean 
gnomemeeting_local_addressbook_has_url (GmAddressbook *)
{
  return TRUE;
}


gboolean 
gnomemeeting_local_addressbook_has_speeddial (GmAddressbook *)
{
  return TRUE;
}


gboolean 
gnomemeeting_local_addressbook_has_categories (GmAddressbook *)
{
  return TRUE;
}


gboolean 
gnomemeeting_local_addressbook_has_location (GmAddressbook *)
{
  return FALSE;
}


gboolean 
gnomemeeting_local_addressbook_has_comment (GmAddressbook *)
{
  return FALSE;
}


gboolean 
gnomemeeting_local_addressbook_has_software (GmAddressbook *)
{
  return FALSE;
}


gboolean 
gnomemeeting_local_addressbook_has_email (GmAddressbook *)
{
  return TRUE;
}


gboolean gnomemeeting_local_addressbook_has_state (GmAddressbook *)
{
  return FALSE;
}
