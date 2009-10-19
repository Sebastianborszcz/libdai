/*  This file is part of libDAI - http://www.libdai.org/
 *
 *  libDAI is licensed under the terms of the GNU General Public License version
 *  2, or (at your option) any later version. libDAI is distributed without any
 *  warranty. See the file COPYING for more details.
 *
 *  Copyright (C) 2002       Martijn Leisink  [martijn@mbfys.kun.nl]
 *  Copyright (C) 2006-2009  Joris Mooij      [joris dot mooij at libdai dot org]
 *  Copyright (C) 2002-2007  Radboud University Nijmegen, The Netherlands
 */


/// \file
/// \brief Defines the IndexFor, multifor, Permute and State classes
/// \todo Improve documentation of IndexFor


#ifndef __defined_libdai_index_h
#define __defined_libdai_index_h


#include <vector>
#include <algorithm>
#include <map>
#include <dai/varset.h>


namespace dai {


/// Tool for looping over the states of several variables.
/** The class IndexFor is an important tool for indexing Factor entries.
 *  Its usage can best be explained by an example.
 *  Assume indexVars, forVars are both VarSets.
 *  Then the following code:
 *  \code
 *      IndexFor i( indexVars, forVars );
 *      for( ; i >= 0; ++i ) {
 *          // use long(i)
 *      }
 *  \endcode
 *  loops over all joint states of the variables in forVars,
 *  and (long)i is equal to the linear index of the corresponding
 *  state of indexVars, where the variables in indexVars that are
 *  not in forVars assume their zero'th value.
 *  \idea Optimize all indices as follows: keep a cache of all (or only
 *  relatively small) indices that have been computed (use a hash). Then,
 *  instead of computing on the fly, use the precomputed ones.
 */
class IndexFor {
    private:
        /// The current linear index corresponding to the state of indexVars
        long                _index;

        /// For each variable in forVars, the amount of change in _index
        std::vector<long>   _sum;

        /// For each variable in forVars, the current state
        std::vector<size_t> _count;

        /// For each variable in forVars, its number of possible values
        std::vector<size_t> _ranges;

    public:
        /// Default constructor
        IndexFor() {
            _index = -1;
        }

        /// Constructor
        IndexFor( const VarSet& indexVars, const VarSet& forVars ) : _count( forVars.size(), 0 ) {
            long sum = 1;

            _ranges.reserve( forVars.size() );
            _sum.reserve( forVars.size() );

            VarSet::const_iterator j = forVars.begin();
            for( VarSet::const_iterator i = indexVars.begin(); i != indexVars.end(); ++i ) {
                for( ; j != forVars.end() && *j <= *i; ++j ) {
                    _ranges.push_back( j->states() );
                    _sum.push_back( (*i == *j) ? sum : 0 );
                }
                sum *= i->states();
            }
            for( ; j != forVars.end(); ++j ) {
                _ranges.push_back( j->states() );
                _sum.push_back( 0 );
            }
            _index = 0;
        }

        /// Sets the index back to zero
        IndexFor& clear() {
            fill( _count.begin(), _count.end(), 0 );
            _index = 0;
            return( *this );
        }

        /// Conversion to long
        operator long () const {
            return( _index );
        }

        /// Pre-increment operator
        IndexFor& operator++ () {
            if( _index >= 0 ) {
                size_t i = 0;

                while( i < _count.size() ) {
                    _index += _sum[i];
                    if( ++_count[i] < _ranges[i] )
                        break;
                    _index -= _sum[i] * _ranges[i];
                    _count[i] = 0;
                    i++;
                }

                if( i == _count.size() )
                    _index = -1;
            }
            return( *this );
        }
};


/// Tool for calculating permutations of linear indices of multi-dimensional arrays.
class Permute {
    private:
        /// Stores the number of possible values of all indices
        std::vector<size_t>  _ranges;
        /// Stores the permutation
        std::vector<size_t>  _sigma;

    public:
        /// Default constructor
        Permute() : _ranges(), _sigma() {}

        /// Construct from vector of index ranges and permutation
        Permute( const std::vector<size_t> &rs, const std::vector<size_t> &sigma ) : _ranges(rs), _sigma(sigma) {
            DAI_ASSERT( _ranges.size() == _sigma.size() );
        }

        /// Construct from vector of variables.
        /** The implied permutation maps the index of each variable in \a vars according to the canonical ordering 
         *  (i.e., sorted ascendingly according to their label) to the index it has in \a vars.
         */
        Permute( const std::vector<Var> &vars ) : _ranges(vars.size()), _sigma(vars.size()) {
            for( size_t i = 0; i < vars.size(); ++i )
                _ranges[i] = vars[i].states();
            VarSet vs( vars.begin(), vars.end(), vars.size() );
            VarSet::const_iterator vs_i = vs.begin();
            for( size_t i = 0; i < vs.size(); ++i, ++vs_i )
                _sigma[i] = find( vars.begin(), vars.end(), *vs_i ) - vars.begin();
        }

        /// Calculates a permuted linear index.
        /** Converts the linear index \a li to a vector index, permutes its 
         *  components, and converts it back to a linear index.
         */
        size_t convertLinearIndex( size_t li ) const {
            size_t N = _ranges.size();

            // calculate vector index corresponding to linear index
            std::vector<size_t> vi;
            vi.reserve( N );
            size_t prod = 1;
            for( size_t k = 0; k < N; k++ ) {
                vi.push_back( li % _ranges[k] );
                li /= _ranges[k];
                prod *= _ranges[k];
            }

            // convert permuted vector index to corresponding linear index
            prod = 1;
            size_t sigma_li = 0;
            for( size_t k = 0; k < N; k++ ) {
                sigma_li += vi[_sigma[k]] * prod;
                prod *= _ranges[_sigma[k]];
            }

            return sigma_li;
        }

        // OBSOLETE
        /// For backwards compatibility (to be removed soon)
        size_t convert_linear_index( size_t li ) const { return convertLinearIndex(li); }

        /// Returns const reference to the permutation
        const std::vector<size_t>& sigma() const { return _sigma; }

        /// Returns reference to the permutation
        std::vector<size_t>& sigma() { return _sigma; }

        /// Returns the result of applying the permutation on \a i
        size_t operator[]( size_t i ) const {
#ifdef DAI_DEBUG
            return _sigma.at(i);
#else
            return _sigma[i];
#endif
        }
};


/// multifor makes it easy to perform a dynamic number of nested \c for loops.
/** An example of the usage is as follows:
 *  \code
 *  std::vector<size_t> ranges;
 *  ranges.push_back( 3 );
 *  ranges.push_back( 4 );
 *  ranges.push_back( 5 );
 *  for( multifor s(ranges); s.valid(); ++s )
 *      cout << "linear index: " << (size_t)s << " corresponds to indices " << s[2] << ", " << s[1] << ", " << s[0] << endl;
 *  \endcode
 *  which would be equivalent to:
 *  \code
 *  size_t s = 0;
 *  for( size_t s2 = 0; s2 < 5; s2++ )
 *      for( size_t s1 = 0; s1 < 4; s1++ )
 *          for( size_t s0 = 0; s0 < 3; s++, s0++ )
 *              cout << "linear index: " << (size_t)s << " corresponds to indices " << s2 << ", " << s1 << ", " << s0 << endl;
 *  \endcode
 */
class multifor {
    private:
        /// Stores the number of possible values of all indices
        std::vector<size_t>  _ranges;
        /// Stores the current values of all indices
        std::vector<size_t>  _indices;
        /// Stores the current linear index
        long                 _linear_index;

    public:
        /// Default constructor
        multifor() : _ranges(), _indices(), _linear_index(0) {}

        /// Initialize from vector of index ranges
        multifor( const std::vector<size_t> &d ) : _ranges(d), _indices(d.size(),0), _linear_index(0) {}

        /// Returns linear index (i.e., the index in the Cartesian product space)
        operator size_t() const {
            DAI_DEBASSERT( valid() );
            return( _linear_index );
        }

        /// Returns \a k 'th index
        size_t operator[]( size_t k ) const {
            DAI_DEBASSERT( valid() );
            DAI_DEBASSERT( k < _indices.size() );
            return _indices[k];
        }

        /// Increments the current indices (prefix)
        multifor & operator++() {
            if( valid() ) {
                _linear_index++;
                size_t i;
                for( i = 0; i != _indices.size(); i++ ) {
                    if( ++(_indices[i]) < _ranges[i] )
                        break;
                    _indices[i] = 0;
                }
                if( i == _indices.size() )
                    _linear_index = -1;
            }
            return *this;
        }

        /// Increments the current indices (postfix)
        void operator++( int ) {
            operator++();
        }

        /// Returns \c true if the current indices are valid
        bool valid() const {
            return( _linear_index >= 0 );
        }
};


/// Makes it easy to iterate over all possible joint states of variables within a VarSet.
/** A joint state of several variables can be represented in two different ways, by a map that maps each variable
 *  to its own state, or by an integer that gives the index of the joint state in the canonical enumeration.
 *
 *  Both representations are useful, and the main functionality provided by the State class is to simplify iterating
 *  over the various joint states of a VarSet and to provide access to the current state in both representations.
 *
 *  \note The same functionality could be achieved by simply iterating over the linear state and using VarSet::calcStates,
 *  but the State class offers a more efficient implementation.
 *
 *  \note A State is very similar to a \link multifor \endlink, but tailored for Var 's and VarSet 's.
 *
 *  \see VarSet::calcState, VarSet::calcStates
 */
class State {
    private:
        /// Type for representing a joint state of some variables as a map, which maps each variable to its state
        typedef std::map<Var, size_t> states_type;

        /// Current state (represented linearly)
        long                          state;

        /// Current state (represented as a map)
        states_type                   states;

    public:
        /// Default constructor
        State() : state(0), states() {}

        /// Initialize from VarSet, resetting the current state
        State( const VarSet &vs ) : state(0) {
            for( VarSet::const_iterator v = vs.begin(); v != vs.end(); v++ )
                states[*v] = 0;
        }

        /// Return current linear state
        operator size_t() const {
            DAI_ASSERT( valid() );
            return( state );
        }

        /// Return current state represented as a map
        const states_type& get() const { return states; }

        /// Return current state of variable \a v, or 0 if \a v is not in \c *this
        size_t operator() ( const Var &v ) const {
            DAI_ASSERT( valid() );
            states_type::const_iterator entry = states.find( v );
            if( entry == states.end() )
                return 0;
            else
                return entry->second;
        }

        /// Return linear state of variables in \a vs, assuming that variables that are not in \c *this are in state 0
        size_t operator() ( const VarSet &vs ) const {
            DAI_ASSERT( valid() );
            size_t vs_state = 0;
            size_t prod = 1;
            for( VarSet::const_iterator v = vs.begin(); v != vs.end(); v++ ) {
                states_type::const_iterator entry = states.find( *v );
                if( entry != states.end() )
                    vs_state += entry->second * prod;
                prod *= v->states();
            }
            return vs_state;
        }

        /// Increments the current state (prefix)
        void operator++( ) {
            if( valid() ) {
                state++;
                states_type::iterator entry = states.begin();
                while( entry != states.end() ) {
                    if( ++(entry->second) < entry->first.states() )
                        break;
                    entry->second = 0;
                    entry++;
                }
                if( entry == states.end() )
                    state = -1;
            }
        }

        /// Increments the current state (postfix)
        void operator++( int ) {
            operator++();
        }

        /// Returns \c true if the current state is valid
        bool valid() const {
            return( state >= 0 );
        }

        /// Resets the current state (to the joint state represented by linear state 0)
        void reset() {
            state = 0;
            for( states_type::iterator s = states.begin(); s != states.end(); s++ )
                s->second = 0;
        }
};


} // end of namespace dai


/** \example example_permute.cpp
 *  This example shows how to use the Permute, multifor and State classes.
 *
 *  \section Output
 *  \verbinclude examples/example_permute.out
 *
 *  \section Source
 */


#endif
