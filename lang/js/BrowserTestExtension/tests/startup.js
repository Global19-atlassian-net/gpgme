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

 describe('GPGME context', function(){
    it('Starting a GpgME instance', function(done){
        let prm = Gpgmejs.init();
        prm.then(
         function(context){
            expect(context.connection).to.not.be.undefined;
            expect(context).to.be.an('object');
            expect(context.connection).to.be.an('object');
            expect(context.Keyring).to.be.undefined;
            expect(context.encrypt).to.be.a('function');
            expect(context.decrypt).to.be.a('function');
            done();
        });
    });
});

describe('GPGME does not start with invalid parameters', function(){
    for (let i=0; i < inputvalues.init.invalid_startups.length; i++){
        it('Parameter '+ i, function(done){
        let prm = Gpgmejs.init(inputvalues.init.invalid_startups[i]);
            prm.then(function(context){
                expect(context).to.be.undefined;
                done();
            }, function(error){
                expect(error).to.be.an.instanceof(Error);
                expect(error.code).to.equal('PARAM_WRONG');
                done();
            });
        })
    }
});