/*
   vdev: a virtual device manager for *nix
   Copyright (C) 2014  Jude Nelson

   This program is dual-licensed: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3 or later as 
   published by the Free Software Foundation. For the terms of this 
   license, see LICENSE.LGPLv3+ or <http://www.gnu.org/licenses/>.

   You are free to use this program under the terms of the GNU General
   Public License, but WITHOUT ANY WARRANTY; without even the implied 
   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   See the GNU General Public License for more details.

   Alternatively, you are free to use this program under the terms of the 
   Internet Software Consortium License, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   For the terms of this license, see LICENSE.ISC or 
   <http://www.isc.org/downloads/software-support-policy/isc-license/>.
*/
#include "acl.h"

#define INI_MAX_LINE 4096 
#define INI_STOP_ON_FIRST_ERROR 1

#include "ini.h"
#include "vdev.h"
#include "match.h"


// parse the ACL mode 
static int vdev_acl_parse_mode( mode_t* mode, char const* mode_str ) {
   
   char* tmp = NULL;
   
   *mode = ((mode_t)strtol( mode_str, &tmp, 8 )) & 0777;
   
   if( *tmp != '\0' ) {
      return -EINVAL;
   }
   else {
      return 0;
   }
}


// convert a printable sha256 to a binary sha256 of length SHA256_DIGEST_LENGTH
// return 0 on success and fill in *sha256_bin to point to an allocated buffer
// return negative on error
static int vdev_sha256_to_bin( char const* sha256_printable, unsigned char** sha256_bin ) {
   
   unsigned char* ret = NULL;
   
   // sanity check 
   if( strlen(sha256_printable) != 2 * SHA256_DIGEST_LENGTH ) {
      return -EINVAL;
   }
   
   // sanity check
   for( int i = 0; i < 2 * SHA256_DIGEST_LENGTH; i++ ) {
      
      int c = sha256_printable[i];
      
      // must be a hex number 
      if( ! ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) ) {
         return -EINVAL;
      }
   }
   
   ret = VDEV_CALLOC( unsigned char, SHA256_DIGEST_LENGTH );
   
   if( ret == NULL ) {
      return -ENOMEM;
   }
   
   for( int i = 0; i < SHA256_DIGEST_LENGTH; i++ ) {
      
      char buf[2];
      
      memcpy( buf, sha256_printable + (2 * i), 2 );
      
      ret[i] = (unsigned char) strtol( buf, NULL, 16 );
   }
   
   *sha256_bin = ret;
   
   return 0;
}



// callback from inih to parse an acl 
// return 1 on success
// return 0 on failure
static int vdev_acl_ini_parser( void* userdata, char const* section, char const* name, char const* value ) {
   
   struct vdev_acl* acl = (struct vdev_acl*)userdata;
   int rc = 0;
   
   // verify this is an acl section 
   if( strcmp(section, VDEV_ACL_NAME) != 0 ) {
      
      fprintf(stderr, "Invalid section '%s'\n", section);
      return 0;
   }
   
   if( strcmp(name, VDEV_ACL_NAME_UID ) == 0 ) {
      
      // parse user or UID 
      uid_t uid = 0;
      int rc = vdev_parse_uid( value, &uid );
      
      if( rc != 0 ) {
         vdev_error("vdev_parse_uid(%s) rc = %d\n", value, rc );
         
         fprintf(stderr, "Invalid user/UID '%s'\n", value );
         return 0;
      }
      
      acl->has_uid = true;
      acl->uid = uid;
      return 1;
   }
      
   if( strcmp(name, VDEV_ACL_NAME_SETUID ) == 0 ) {
      
      // parse user or UID 
      uid_t uid = 0;
      int rc = vdev_parse_uid( value, &uid );
      
      if( rc != 0 ) {
         vdev_error("vdev_parse_uid(%s) rc = %d\n", value, rc );
         
         fprintf(stderr, "Invalid user/UID '%s'\n", value );
         return 0;
      }
      
      acl->has_setuid = true;
      acl->setuid = uid;
      return 1;
   }
   
   if( strcmp(name, VDEV_ACL_NAME_GID ) == 0 ) {
      
      // parse group or GID 
      gid_t gid = 0;
      int rc = vdev_parse_gid( value, &gid );
      
      if( rc != 0 ) {
         vdev_error("vdev_parse_gid(%s) rc = %d\n", value, rc );
         
         fprintf(stderr, "Invalid group/GID '%s'\n", value );
         return 0;
      }
      
      acl->has_gid = true;
      acl->gid = gid;
      return 1;
   }
   
   if( strcmp(name, VDEV_ACL_NAME_SETGID ) == 0 ) {
      
      // parse group or GID 
      gid_t gid = 0;
      int rc = vdev_parse_gid( value, &gid );
      
      if( rc != 0 ) {
         vdev_error("vdev_parse_gid(%s) rc = %d\n", value, rc );
         
         fprintf(stderr, "Invalid group/GID '%s'\n", value );
         return 0;
      }
      
      acl->has_setgid = true;
      acl->setgid = gid;
      return 1;
   }
   
   if( strcmp(name, VDEV_ACL_NAME_SETMODE ) == 0 ) {
      
      // parse mode 
      mode_t mode = 0;
      int rc = vdev_acl_parse_mode( &mode, value );
      
      if( rc != 0 ) {
         vdev_error("vdev_acl_parse_mode(%s) rc = %d\n", value, rc );
         
         fprintf(stderr, "Invalid mode '%s'\n", value );
         return 0;
      }
      
      acl->has_setmode = true;
      acl->setmode = mode;
      return 1;
   }
   
   if( strcmp(name, VDEV_ACL_DEVICE_REGEX ) == 0 ) {
      
      // parse and preserve this value 
      int rc = vdev_match_regex_append( &acl->paths, &acl->regexes, &acl->num_paths, value );
      
      if( rc != 0 ) {
         vdev_error("vdev_match_regex_append(%s) rc = %d\n", value, rc );
         
         fprintf(stderr, "Invalid regex '%s'\n", value );
         return 0;
      }
      
      return 1;
   }
   
   if( strcmp(name, VDEV_ACL_NAME_PROC_PATH ) == 0 ) {
      
      // preserve this value 
      if( acl->proc_path != NULL ) {
         
         fprintf(stderr, "Duplicate process path '%s'\n", value );
         return 0;
      }
      
      acl->has_proc = true;
      acl->proc_path = vdev_strdup_or_null(value);
      return 1;
   }
   
   if( strcmp(name, VDEV_ACL_NAME_PROC_SHA256 ) == 0 ) {
      
      // preserve this value 
      if( acl->proc_sha256 != NULL ) {
         
         fprintf(stderr, "Duplicate process SHA256 '%s'\n", value );
         return 0;
      }
      
      rc = vdev_sha256_to_bin( value, &acl->proc_sha256 );
      
      if( rc != 0 ) {
         fprintf(stderr, "Failed to parse '%s' as a SHA256 hash\n", value );
         return 0;
      }
      
      acl->has_proc = true;
      
      return 1;
   }
   
   if( strcmp(name, VDEV_ACL_NAME_PROC_PIDLIST ) == 0 ) {
      
      // preserve this value 
      if( acl->proc_pidlist_cmd != NULL ) {
         
         fprintf(stderr, "Duplicate process PID list command '%s'\n", value );
         return 0;
      }
      
      acl->has_proc = true;
      acl->proc_pidlist_cmd = vdev_strdup_or_null(value);
      return 1;
   }
   
   if( strcmp(name, VDEV_ACL_NAME_PROC_INODE ) == 0 ) {
      
      // preserve this value 
      ino_t inode = 0;
      bool success = false;
      
      inode = vdev_parse_uint64( value, &success );
      if( !success ) {
         
         fprintf(stderr, "Failed to parse inode '%s'\n", value );
         return 0;
      }
      
      acl->has_proc = true;
      acl->proc_inode = inode;
      return 1;
   }
   
   fprintf(stderr, "Unrecognized field '%s'\n", name);
   return 0;
}


// acl sanity check 
int vdev_acl_sanity_check( struct vdev_acl* acl ) {
   
   int rc = 0;
   
   if( acl->has_gid ) {
      
      rc = vdev_validate_gid( acl->gid );
      if( rc != 0 ) {
         
         fprintf(stderr, "Invalid GID %d\n", acl->gid );
         return rc;
      }
   }
   
   if( acl->has_setgid ) {
      
      rc = vdev_validate_gid( acl->gid );
      if( rc != 0 ) {
         
         fprintf(stderr, "Invalid set-GID %d\n", acl->setgid );
         return rc;
      }
   }
   
   if( acl->has_uid ) {
      
      rc = vdev_validate_uid( acl->uid );
      if( rc != 0 ) {
         
         fprintf(stderr, "Invalid UID %d\n", acl->uid );
         return rc;
      }
   }
   
   if( acl->has_setuid ) {
      
      rc = vdev_validate_uid( acl->uid );
      if( rc != 0 ) {
         
         fprintf(stderr, "Invalid set-UID %d\n", acl->setuid );
         return rc;
      }
   }
   
   return rc;
}

// initialize an acl 
int vdev_acl_init( struct vdev_acl* acl ) {
   memset( acl, 0, sizeof(struct vdev_acl) );
   return 0;
}


// free an acl 
int vdev_acl_free( struct vdev_acl* acl ) {
   
   if( acl->num_paths > 0 ) {
      
      vdev_match_regexes_free( acl->paths, acl->regexes, acl->num_paths );
   }
   
   if( acl->proc_path != NULL ) {
      free( acl->proc_path );
      acl->proc_path = NULL;
   }
   
   if( acl->proc_sha256 != NULL ) {
      free( acl->proc_sha256 );
      acl->proc_sha256 = NULL;
   }
   
   return 0;
}


// load an ACL from a file handle
int vdev_acl_load_file( FILE* file, struct vdev_acl* acl ) {
   
   int rc = 0;
   
   vdev_acl_init( acl );
   
   rc = ini_parse_file( file, vdev_acl_ini_parser, acl );
   if( rc != 0 ) {
      vdev_error("ini_parse_file(ACL) rc = %d\n", rc );
      return rc;
   }
   
   // sanity check 
   rc = vdev_acl_sanity_check( acl );
   if( rc != 0 ) {
      
      vdev_error("vdev_acl_sanity_check rc = %d\n", rc );
      
      vdev_acl_free( acl );
      memset( acl, 0, sizeof(struct vdev_acl) );
      return rc;
   }
   
   return rc;
}


// load an ACL from a file, given the path
int vdev_acl_load( char const* path, struct vdev_acl* acl ) {
   
   int rc = 0;
   FILE* f = NULL;
   
   f = fopen( path, "r" );
   if( f == NULL ) {
      
      rc = -errno;
      vdev_error("fopen(%s) errno = %d\n", rc );
      return rc;
   }
   
   rc = vdev_acl_load_file( f, acl );
   
   fclose( f );
   
   if( rc != 0 ) {
      vdev_error("vdev_acl_load_file(%s) rc = %d\n", path, rc );
   }
   
   return rc;
}


// free a vector of acls
static void vdev_acl_free_vector( vector<struct vdev_acl>* acls ) {
   
   for( unsigned int i = 0; i < acls->size(); i++ ) {
      
      vdev_acl_free( &acls->at(i) );
   }
   
   acls->clear();
}

// free a C-style list of acls (including the list itself)
int vdev_acl_free_all( struct vdev_acl* acl_list, size_t num_acls ) {
   
   int rc = 0;
   
   for( unsigned int i = 0; i < num_acls; i++ ) {
      
      rc = vdev_acl_free( &acl_list[i] );
      if( rc != 0 ) {
         
         return rc;
      }
   }
   
   free( acl_list );
   
   return rc;
}


// loader for an acl 
int vdev_acl_loader( char const* fp, void* cls ) {
   
   struct vdev_acl acl;
   int rc = 0;
   struct stat sb;
   
   // skip if not a regular file 
   rc = stat( fp, &sb );
   if( rc != 0 ) {
      
      rc = -errno;
      vdev_error("stat(%s) rc = %d\n", fp, rc );
      return rc;
   }
   
   if( !S_ISREG( sb.st_mode ) ) {
      
      return 0;
   }
   
   vdev_debug("Load ACL %s\n", fp );
   
   vector<struct vdev_acl>* acls = (vector<struct vdev_acl>*)cls;

   memset( &acl, 0, sizeof(struct vdev_acl) );
   
   rc = vdev_acl_load( fp, &acl );
   if( rc != 0 ) {
      
      vdev_error("vdev_acl_load(%s) rc = %d\n", fp, rc );
      return rc;
   }
   
   // save this acl 
   try {
      acls->push_back( acl );
   }
   catch( bad_alloc& ba ) {
      
      return -ENOMEM;
   }
   
   return 0;
}

// load all ACLs from a directory, in lexographic order 
// return 0 on success, negative on error
int vdev_acl_load_all( char const* dir_path, struct vdev_acl** ret_acls, size_t* ret_num_acls ) {
   
   int rc = 0;
   vector<struct vdev_acl> acls;
   
   rc = vdev_load_all( dir_path, vdev_acl_loader, &acls );
   
   if( rc != 0 ) {
      
      vdev_acl_free_vector( &acls );
      return rc;
   }
   else {
         
      if( acls.size() == 0 ) {
         
         // nothing 
         *ret_acls = NULL;
         *ret_num_acls = 0;
      }
      else {
      
         // extract values
         struct vdev_acl* acl_list = VDEV_CALLOC( struct vdev_acl, acls.size() );
         if( acl_list == NULL ) {
            
            vdev_acl_free_vector( &acls );
            return -ENOMEM;   
         }
         
         // NOTE: vectors are contiguous in memory 
         memcpy( acl_list, &acls[0], sizeof(struct vdev_acl) * acls.size() );
         
         *ret_acls = acl_list;
         *ret_num_acls = acls.size();
      }
   }
   
   return 0;
}

// modify a stat buffer to apply a user access control list, if the acl says so
int vdev_acl_do_set_user( struct vdev_acl* acl, uid_t caller_uid, struct stat* sb ) {
   
   if( acl->has_setuid && acl->has_uid ) {
      
      // change the UID of this path
      if( acl->uid == caller_uid ) {
         sb->st_uid = acl->setuid;
      }
   }
   
   return 0;
}

// modify a stat buffer to apply a group access control list 
int vdev_acl_do_set_group( struct vdev_acl* acl, gid_t caller_gid, struct stat* sb ) {
   
   if( acl->has_setgid && acl->has_gid ) {
      
      // change the GID of this path
      if( acl->gid == caller_gid ) {
         sb->st_gid = acl->setgid;
      }
   }
   
   return 0;
}

// modify a stat buffer to apply the mode
int vdev_acl_do_set_mode( struct vdev_acl* acl, struct stat* sb ) {
   
   if( acl->has_setmode ) {
      
      // clear permission bits 
      sb->st_mode &= ~(0777);
      
      // set permission bits 
      sb->st_mode |= acl->setmode;
   }
   
   return 0;
}


// parse the list of PIDs
// return 0 on success, and allocate pids and set the number of pids in num_pids
// return negative on error
int vdev_parse_pids( char const* pid_list_str, size_t pid_list_len, pid_t** pids, int* num_pids ) {
   
   char* tok_tmp = NULL;
   char* strtoll_tmp = NULL;
   char* pid_list_str_dup = NULL;
   char* pid_list_str_in = NULL;
   char* tok = NULL;
   vector<pid_t> parsed_pids;
   int rc = 0;
   
   // make a copy of the pid list, so we can destroy it
   pid_list_str_dup = VDEV_CALLOC( char, pid_list_len );
   if( pid_list_str_dup == NULL ) {
      return -ENOMEM;
   }
   
   memcpy( pid_list_str_dup, pid_list_str, pid_list_len );
   pid_list_str_in = pid_list_str_dup;
   
   while( true ) {
      
      tok = strtok_r( pid_list_str_in, " \n\r\t", &tok_tmp );
      pid_list_str_in = NULL;
      
      if( tok == NULL ) {
         // out of tokens
         break;
      }
      
      // is this a pid?
      pid_t next_pid = (pid_t)strtoll( tok, &strtoll_tmp, 10 );
      if( *strtoll_tmp != '\0' ) {
         
         // not valid 
         vdev_error("Invalid PID '%s'\n", tok );
         rc = -EINVAL;
         break;
      }
      
      parsed_pids.push_back( next_pid );
   }
   
   free( pid_list_str_dup );
   
   if( rc == 0 ) {
      
      *pids = VDEV_CALLOC( pid_t, parsed_pids.size() );
      if( *pids == NULL ) {
         return -ENOMEM;
      }
      
      // NOTE: vectors are guaranteed to be laid out contiguously in memory...
      memcpy( *pids, &parsed_pids[0], sizeof(pid_t) * parsed_pids.size() );
   }
   
   return 0;
}


// run a 'pidlist' command, with the appropriate environment variables set.
// populate *pids with the list of PIDs returned, on success
// return 0 on success
// return negative on error 
int vdev_acl_do_pidlist( struct vdev_acl* acl, struct pstat* ps, uid_t caller_uid, gid_t caller_gid, pid_t** ret_pids, int* ret_num_pids ) {

   // get the list of PIDs from the shell command 
   char* pid_list_str = NULL;
   size_t pid_list_maxlen = VDEV_ACL_PROC_BUFLEN;
   pid_t* pids = NULL;
   int num_pids = 0;
   int exit_status = 0;
   char* cmd_buf = NULL;
   char env_buf[3][100];
   char* pidlist_env[4];
   int rc = 0;
   
   // build the command with the apporpriate environment variables:
   // * VDEV_GID: the gid of the calling process
   // * VDEV_UID: the uid of the calling process
   // * VDEV_PID: the pid of the calling process
   
   sprintf(env_buf[0], "VDEV_UID=%u", caller_uid );
   sprintf(env_buf[1], "VDEV_GID=%u", caller_gid );
   sprintf(env_buf[2], "VDEV_PID=%u", ps->pid );
   
   pidlist_env[0] = env_buf[0];
   pidlist_env[1] = env_buf[1];
   pidlist_env[2] = env_buf[2];
   pidlist_env[3] = NULL;
   
   rc = vdev_subprocess( cmd_buf, pidlist_env, &pid_list_str, pid_list_maxlen, &exit_status );
   
   if( rc != 0 ) {
      
      vdev_error("vdev_subprocess('%s') rc = %d\n", cmd_buf, rc );
      
      free( cmd_buf );
      return rc;
   }
   
   if( exit_status != 0 ) {
      
      vdev_error("vdev_subprocess('%s') exit status %d\n", cmd_buf, exit_status );
      free( cmd_buf );
      return -EPERM;
   }
   
   free( cmd_buf );
   
   // lex the pids
   rc = vdev_parse_pids( pid_list_str, pid_list_maxlen, &pids, &num_pids );
   
   free( pid_list_str );
   
   if( rc != 0 ) {
      
      vdev_error("vdev_parse_pids rc = %d\n", rc );
      return rc;
   }
   
   *ret_pids = pids;
   *ret_num_pids = num_pids;
   
   return 0;
}
      

// check that the caller PID is matched by the given ACL.
// every process match criterion must be satisfied.
// return 1 if all ACL criteria match
// return 0 if at least one ACL criterion does not match 
// return negative on error
int vdev_acl_match_process( struct vdev_acl* acl, struct pstat* ps, uid_t caller_uid, gid_t caller_gid ) {
   
   int rc = 0;
   
   if( !acl->has_proc ) {
      // applies to anyone 
      return 1;
   }
   
   if( acl->proc_path != NULL || acl->proc_sha256 != NULL || acl->has_proc_inode ) {
      
      if( acl->proc_path != NULL ) {
         if( strcmp( acl->proc_path, ps->path ) != 0 ) {
            
            // doesn't match 
            return 0;
         }
      }
      
      if( acl->proc_sha256 != NULL ) {
         
         // do we need to get the SHA256?
         unsigned char blank_sha256[SHA256_DIGEST_LENGTH];
         memset( blank_sha256, 0, SHA256_DIGEST_LENGTH );
         
         if( memcmp( blank_sha256, ps->sha256, SHA256_DIGEST_LENGTH ) == 0 ) {
            
            // we don't have the hash.  Go get it
            rc = pstat_sha256( ps );
            if( rc != 0 ) {
               
               vdev_error("Failed to SHA256 the process binary for %d, rc = %d\n", ps->pid, rc );
               return rc;
            }
         }
         
         if( memcmp( acl->proc_sha256, ps->sha256, SHA256_DIGEST_LENGTH ) != 0 ) {
            
            // doesn't match 
            return 0;
         }
      }
      
      if( acl->has_proc_inode ) {
         if( acl->proc_inode != ps->bin_stat.st_ino ) {
            
            // doesn't match 
            return 0;
         }
      }
   }
   
   // filter on a given PID list generator?
   if( acl->proc_pidlist_cmd != NULL ) {
      
      // get the list of PIDs from the shell command 
      pid_t* pids = NULL;
      int num_pids = 0;
      bool found = false;
      
      rc = vdev_acl_do_pidlist( acl, ps, caller_uid, caller_gid, &pids, &num_pids );
      if( rc != 0 ) {
         
         vdev_error("vdev_acl_do_pidlist('%s') rc = %d\n", acl->proc_pidlist_cmd, rc );
         return rc;
      }
      
      // is the caller in the list?
      for( int i = 0; i < num_pids; i++ ) {
         
         if( ps->pid == pids[i] ) {
            found = true;
            break;
         }
      }
      
      free( pids );
      
      if( !found ) {
         
         return 0;
      }
   }
   
   // all checks match
   return 1;
}


// given a list of access control lists, find the index of the first one that applies to the given caller and path. 
// return >= 0 with the index
// return num_acls if not found
// return negative on error
int vdev_acl_find_next( char const* path, struct pstat* caller_proc, uid_t caller_uid, gid_t caller_gid, struct vdev_acl* acls, size_t num_acls ) {
   
   int rc = 0;
   bool found = false;
   int idx = 0;
   
   for( unsigned int i = 0; i < num_acls; i++ ) {
      
      rc = 0;
      
      // match UID?
      if( acls[i].has_uid && acls[i].uid != caller_uid ) {
         // nope 
         continue;
      }
      
      // match GID?
      if( acls[i].has_gid && acls[i].gid != caller_gid ) {
         // nope 
         continue;
      }
      
      // match path?
      if( acls[i].num_paths > 0 ) {
         
         rc = vdev_match_first_regex( path, acls[i].regexes, acls[i].num_paths );
         
         if( rc >= (signed)acls[i].num_paths ) {
            // no match
            continue;
         }
      }
      
      if( rc < 0 ) {
         
         vdev_error("vdev_match_first_regex(%s) rc = %d\n", path, rc );
         break;
      }
      
      // match process?  Do this last, since it can be expensive 
      rc = vdev_acl_match_process( &acls[i], caller_proc, caller_uid, caller_gid );
      if( rc == 0 ) {
         // no match 
         continue;
      }
      
      if( rc < 0 ) {
         
         // error...
         vdev_error("vdev_acl_match_process(%d) rc = %d\n", caller_proc->pid, rc );
         break;
      }
      
      // success!
      found = true;
      idx = i;
      rc = 0;
      break;
   }
   
   if( found ) {
      return idx;
   }
   else {
      if( rc >= 0 ) {
         return num_acls;
      }
      else {
         return rc;
      }
   }
}

// apply the acl to the stat buf, filtering on caller uid, gid, and process information
int vdev_acl_apply( struct vdev_acl* acl, uid_t caller_uid, gid_t caller_gid, struct stat* sb ) {
   
   // set user, group, mode (if given)
   vdev_acl_do_set_user( acl, caller_uid, sb );
   vdev_acl_do_set_group( acl, caller_gid, sb );
   vdev_acl_do_set_mode( acl, sb );
   
   return 0;
}


// go through the list of acls and apply any modifications to the given stat buffer
// return 1 if at least one ACL matches
// return 0 if there are no matches (i.e. this device node should be hidden)
// return negative on error 
int vdev_acl_apply_all( struct vdev_acl* acls, size_t num_acls, char const* path, struct pstat* caller_proc, uid_t caller_uid, gid_t caller_gid, struct stat* sb ) {
   
   int rc = 0;
   int acl_offset = 0;
   int i = 0;
   bool found = false;
   
   while( acl_offset < (signed)num_acls ) {
      
      // find the next acl 
      rc = vdev_acl_find_next( path, caller_proc, caller_uid, caller_gid, acls + acl_offset, num_acls - acl_offset );
      
      if( rc == (signed)(num_acls - acl_offset) ) {
         
         // not found 
         rc = 0;
         break;
      }
      else if( rc < 0 ) {
         
         vdev_error("vdev_acl_find_next(%s, offset = %d) rc = %d\n", path, acl_offset, rc );
         break;
      }
      else {
         
         // matched! advance offset to next acl
         i = acl_offset + rc;
         acl_offset += rc + 1;
         found = true;
         
         // apply the ACL 
         rc = vdev_acl_apply( &acls[i], caller_uid, caller_gid, sb );
         if( rc != 0 ) {
            
            vdev_error("vdev_acl_apply(%s, offset = %d) rc = %d\n", path, acl_offset, rc );
            break;
         }
      }
   }
   
   if( rc == 0 ) {
      // no error reported 
      if( found ) {
         rc = 1;
      }
   }
   
   return rc;
}

