/* gpgme.js - Javascript integration for gpgme
 * Copyright (C) 2018 Bundesamt für Sicherheit in der Informationstechnik
 *
 * This file is part of GPGME.
 *
 * GPGME is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * GPGME is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 * SPDX-License-Identifier: LGPL-2.1+
 */

/**
 * The key class allows to query the information defined in gpgme Key Objects
 * (see https://www.gnupg.org/documentation/manuals/gpgme/Key-objects.html)
 *
 * This is a stub, as the gpgme-json side is not yet implemented
 *
 */

import { isFingerprint, isLongId } from './Helpers'
import { gpgme_error } from './Errors'
import { createMessage } from './Message';
import { permittedOperations } from './permittedOperations';
import { Connection } from './Connection';

/**
 * Validates the fingerprint, and checks for tha availability of a connection.
 * If both are available, a Key will be returned.
 * @param {String} fingerprint
 * @param {Object} parent Either a Connection, or the invoking object with a
 * Connection (e.g. Keyring)
 */
export function createKey(fingerprint, parent){
    if (!isFingerprint(fingerprint)){
        return gpgme_error('PARAM_WRONG');
    }
    if ( parent instanceof Connection){
        return new GPGME_Key(fingerprint, parent);
    } else if ( parent.hasOwnProperty('connection') &&
        parent.connection instanceof Connection){
            return new GPGME_Key(fingerprint, parent.connection);
    } else {
        return gpgme_error('PARAM_WRONG');
    }
}

/**
 * Representing the Keys as stored in GPG
 */
export class GPGME_Key {

    constructor(fingerprint, connection){
        this.fingerprint = fingerprint;
        this.connection = connection;
    }

    set connection(conn){
        if (this._connection instanceof Connection) {
            gpgme_error('CONN_ALREADY_CONNECTED');
        } else if (conn instanceof Connection ) {
            this._connection = conn;
        }
    }

    get connection(){
        if (!this._data.fingerprint){
            return gpgme_error('KEY_INVALID');
        }
        if (!this._connection instanceof Connection){
            return gpgme_error('CONN_NO_CONNECT');
        } else {
            return this._connection;
        }
    }

    set fingerprint(fpr){
        if (isFingerprint(fpr) === true) {
            if (this._data === undefined) {
                this._data = {fingerprint:  fpr};
            } else {
                if (this._data.fingerprint === undefined){
                    this._data.fingerprint = fpr;
                }
            }
        }
    }

    get fingerprint(){
        if (!this._data || !this._data.fingerprint){
            return gpgme_error('KEY_INVALID');
        }
        return this._data.fingerprint;
    }

    /**
     *
     * @param {Object} data Bulk set data for this key, with the Object as sent
     * by gpgme-json.
     * @returns {GPGME_Key|GPGME_Error} The Key object itself after values have
     * been set
     */
    setKeydata(data){
        if (this._data === undefined) {
            this._data = {};
        }
        if (
            typeof(data) !== 'object') {
            return gpgme_error('KEY_INVALID');
        }
        if (!this._data.fingerprint && isFingerprint(data.fingerprint)){
            if (data.fingerprint !== this.fingerprint){
                return gpgme_error('KEY_INVALID');
            }
            this._data.fingerprint = data.fingerprint;
        } else if (this._data.fingerprint !== data.fingerprint){
            return gpgme_error('KEY_INVALID');
        }

        let booleans = ['expired', 'disabled','invalid','can_encrypt',
            'can_sign','can_certify','can_authenticate','secret',
            'is_qualified'];
        for (let b =0; b < booleans.length; b++) {
            if (
                !data.hasOwnProperty(booleans[b]) ||
                typeof(data[booleans[b]]) !== 'boolean'
            ){
                return gpgme_error('KEY_INVALID');
            }
            this._data[booleans[b]] = data[booleans[b]];
        }
        if (typeof(data.protocol) !== 'string'){
            return gpgme_error('KEY_INVALID');
        }
        // TODO check valid protocols?
        this._data.protocol = data.protocol;

        if (typeof(data.owner_trust) !== 'string'){
            return gpgme_error('KEY_INVALID');
        }
        // TODO check valid values?
        this._data.owner_trust = data.owner_trust;

        // TODO: what about origin ?
        if (!Number.isInteger(data.last_update)){
            return gpgme_error('KEY_INVALID');
        }
        this._data.last_update = data.last_update;

        this._data.subkeys = [];
        if (data.hasOwnProperty('subkeys')){
            if (!Array.isArray(data.subkeys)){
                return gpgme_error('KEY_INVALID');
            }
            for (let i=0; i< data.subkeys.length; i++) {
                this._data.subkeys.push(
                    new GPGME_Subkey(data.subkeys[i]));
            }
        }

        this._data.userids = [];
        if (data.hasOwnProperty('userids')){
            if (!Array.isArray(data.userids)){
                return gpgme_error('KEY_INVALID');
            }
            for (let i=0; i< data.userids.length; i++) {
                this._data.userids.push(
                    new GPGME_UserId(data.userids[i]));
            }
        }
        return this;
    }

    /**
     * Query any property of the Key list
     * (TODO: armor not in here, might be unexpected)
     * @param {String} property Key property to be retreived
     * @param {*} cached (optional) if false, the data will be directly queried
     * from gnupg.
     *  @returns {*|Promise<*>} the value, or if not cached, a Promise
     * resolving on the value
     */
    get(property, cached=true) {
        if (cached === false) {
            let me = this;
            return new Promise(function(resolve, reject) {
                me.refreshKey().then(function(key){
                    resolve(key.get(property, true));
                }, function(error){
                    reject(error);
                });
            });
         } else {
            if (!this._data.hasOwnProperty(property)){
                return gpgme_error('PARAM_WRONG');
            } else {
                return (this._data[property]);
            }
        }
    }

    /**
     * Reloads the Key from gnupg
     */
    refreshKey() {
        let me = this;
        return new Promise(function(resolve, reject) {
            if (!me._data.fingerprint){
                reject(gpgme_error('KEY_INVALID'));
            }
            let msg = createMessage('keylist');
            msg.setParameter('sigs', true);
            msg.setParameter('keys', me._data.fingerprint);
            me.connection.post(msg).then(function(result){
                if (result.keys.length === 1){
                    me.setKeydata(result.keys[0]);
                    resolve(me);
                } else {
                    reject(gpgme_error('KEY_NOKEY'));
                }
            }, function (error) {
                reject(gpgme_error('GNUPG_ERROR'), error);
            })
        });
    }

    //TODO:
    /**
     * Get the armored block of the non- secret parts of the Key.
     * @returns {String} the armored Key block.
     * Notice that this may be outdated cached info. Use the async getArmor if
     * you need the most current info
     */
    // get armor(){ TODO }

    /**
     * Query the armored block of the non- secret parts of the Key directly
     * from gpg.
     * Async, returns Promise<String>
     */
    // getArmor(){ TODO }
    //

    // get hasSecret(){TODO} // confusing difference to Key.get('secret')!
    // getHasSecret(){TODO async version}
}

/**
 * The subkeys of a Key. Currently, they cannot be refreshed separately
 */
class GPGME_Subkey {

    constructor(data){
        let keys = Object.keys(data);
        for (let i=0; i< keys.length; i++) {
            this.setProperty(keys[i], data[keys[i]]);
        }
    }

    setProperty(property, value){
        if (!this._data){
            this._data = {};
        }
        if (validSubKeyProperties.hasOwnProperty(property)){
            if (validSubKeyProperties[property](value) === true) {
                this._data[property] = value;
            }
        }
    }

    /**
     *
     * @param {String} property Information to request
     * @returns {String | Number}
     * TODO: date properties are numbers with Date in seconds
     */
    get(property) {
        if (this._data.hasOwnProperty(property)){
            return (this._data[property]);
        }
    }
}

class GPGME_UserId {

    constructor(data){
        let keys = Object.keys(data);
        for (let i=0; i< keys.length; i++) {
            this.setProperty(keys[i], data[keys[i]]);
        }
    }

    setProperty(property, value){
        if (!this._data){
            this._data = {};
        }
        if (validUserIdProperties.hasOwnProperty(property)){
            if (validUserIdProperties[property](value) === true) {
                this._data[property] = value;
            }
        }
    }

    /**
     *
     * @param {String} property Information to request
     * @returns {String | Number}
     * TODO: date properties are numbers with Date in seconds
     */
    get(property) {
        if (this._data.hasOwnProperty(property)){
            return (this._data[property]);
        }
    }
}

const validUserIdProperties = {
    'revoked': function(value){
        return typeof(value) === 'boolean';
    },
    'invalid':  function(value){
        return typeof(value) === 'boolean';
    },
    'uid': function(value){
        if (typeof(value) === 'string' || value === ''){
            return true;;
        }
        return false;
    },
    'validity': function(value){
        if (typeof(value) === 'string'){
            return true;;
        }
        return false;
    },
    'name': function(value){
        if (typeof(value) === 'string' || value === ''){
        return true;;
        }
        return false;
    },
    'email': function(value){
        if (typeof(value) === 'string' || value === ''){
            return true;;
        }
        return false;
    },
    'address': function(value){
        if (typeof(value) === 'string' || value === ''){
            return true;;
        }
        return false;
    },
    'comment': function(value){
        if (typeof(value) === 'string' || value === ''){
            return true;;
        }
        return false;
    },
    'origin':  function(value){
        return Number.isInteger(value);
    },
    'last_update':  function(value){
        return Number.isInteger(value);
    }
};

const validSubKeyProperties = {
    'invalid': function(value){
        return typeof(value) === 'boolean';
    },
    'can_encrypt': function(value){
        return typeof(value) === 'boolean';
    },
    'can_sign': function(value){
        return typeof(value) === 'boolean';
    },
    'can_certify':  function(value){
        return typeof(value) === 'boolean';
    },
    'can_authenticate':  function(value){
        return typeof(value) === 'boolean';
    },
    'secret': function(value){
        return typeof(value) === 'boolean';
    },
    'is_qualified': function(value){
        return typeof(value) === 'boolean';
    },
    'is_cardkey':  function(value){
        return typeof(value) === 'boolean';
    },
    'is_de_vs':  function(value){
        return typeof(value) === 'boolean';
    },
    'pubkey_algo_name': function(value){
            return typeof(value) === 'string';
            // TODO: check against list of known?['']
    },
    'pubkey_algo_string': function(value){
        return typeof(value) === 'string';
        // TODO: check against list of known?['']
    },
    'keyid': function(value){
        return isLongId(value);
    },
    'pubkey_algo': function(value) {
        return (Number.isInteger(value) && value >= 0);
    },
    'length': function(value){
        return (Number.isInteger(value) && value > 0);
    },
    'timestamp': function(value){
        return (Number.isInteger(value) && value > 0);
    },
    'expires': function(value){
        return (Number.isInteger(value) && value > 0);
    }
}
