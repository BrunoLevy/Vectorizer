/*
 *  Copyright (c) 2000-2022 Inria
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *  * Neither the name of the ALICE Project-Team nor the names of its
 *  contributors may be used to endorse or promote products derived from this
 *  software without specific prior written permission.
 * 
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *  Contact: Bruno Levy
 *
 *     https://www.inria.fr/fr/bruno-levy
 *
 *     Inria,
 *     Domaine de Voluceau,
 *     78150 Le Chesnay - Rocquencourt
 *     FRANCE
 *
 */

// Reference: S. W. Sloan, a fast algorithm for generating
// constrained Delaunay triangulation, 1992,
// Computers and Structures
//
// Specificities of this implementation:
//
// - Edges are systematically manipulated through triangles,
// and these triangles are rotated in-place in the mesh,
// in such a way that the edge we are talking about is
// systematically edge 0 (with vertices 1 and 2).
//
// - The constraint-enforcing step manipulates a queue Q
// of edges encoded this way. It examines pairs of triangles
// t1,t2=Tadj(t1,0), decides whether to swap their common
// edge (based on convexity test and intersection of
// t1's edge 0 with the constraint). In fact, this intersection
// test only depends on the combinatorics of (t1,t2) (two cases)
// and the position of t1' vertex 0 relative to the constraint
// (two cases), that makes 4 cases in total. In these cases,
//    - t1 can either leave Q or be enqueued again
//    - t2 was always in Q already (because it has an edge
//      that has an intersection with the constraint), but
//      there is one case where it leaves Q
// DList has an O(1) function to test whether an element is in the list (using
// flags associated with the elements). It is used in one case: when t2 is
// is not in Q, it means there is no intersection.

#include "CDT_2d.h"
#include <fstream>
#include <cstdlib>

// Used by debugging functions and statistics
#include <set>
#include <deque>

// #define CDT_NAIVE // use naive per-edge method (kept for reference/debugging)

#ifdef GEO_DEBUG
// #define CDT_DEBUG // display *lots* of messages and activates costly checks
#endif

#ifdef CDT_DEBUG
#define CDT_LOG(X) std::cerr << X << std::endl
#else
#define CDT_LOG(X)
#endif

namespace {

    template <class T> 
    inline T det2x2(
        T a11, T a12, T a21, T a22
    ) {
        return a11*a22-a12*a21 ;
    }

    template <class T>
    inline T det3x3(
        T a11, T a12, T a13,                
        T a21, T a22, T a23,                
        T a31, T a32, T a33
    ) {
    return
         a11*det2x2(a22,a23,a32,a33)   
        -a21*det2x2(a12,a13,a32,a33)   
        +a31*det2x2(a12,a13,a22,a23);
    }   
}

namespace GEO {
    
    inline vec2ih operator+(const vec2ih& u, const vec2ih& v) {
        if(u.w == v.w) {
            return vec2ih(
                u.x+v.x,
                u.y+v.y,
                u.w
            );
        }
        return vec2ih(
            v.w*u.x+u.w*v.x,
            v.w*u.y+u.w*v.y,
            u.w*v.w
        );
    }

    inline vec2ih operator-(const vec2ih& u, const vec2ih& v) {
        if(u.w == v.w) {
            return vec2ih(
                u.x-v.x,
                u.y-v.y,
                u.w
            );
        }
        return vec2ih(
            v.w*u.x-u.w*v.x,
            v.w*u.y-u.w*v.y,
            u.w*v.w
        );
    }

    static inline int64_t gcd(int64_t a, int64_t b) {
        a = std::abs(a);
        b = std::abs(b);
        if(a < b) {
            std::swap(a,b);
        }
        int64_t temp;
        while (b != 0) {
            temp = a % b;
            a = b;
            b = temp;
        }
        return a;
    }
    
    inline void simplify(vec2ih& V) {
        if(V.w == 1) {
            return;
        }
        int64_t d = gcd(gcd(V.x,V.w),gcd(V.y,V.w));
        V.x /= d;
        V.y /= d;
        V.w /= d;
    }

    template <class T>
    inline Sign geo_sgn(T x) {
        return (x > T(0)) ? POSITIVE : (
            (x < T(0)) ? NEGATIVE : ZERO
        );
    }

    std::ostream& operator<<(std::ostream& out, const vec2ih& p) {
        // return out << "(" << p.x << "," << p.y << "," << p.w << ")" << " = ("
        // << double(p.x)/double(p.w) << "," << double(p.y)/double(p.w) << ")" ;
        out << double(p.x)/double(p.w) << " " << double(p.y)/double(p.w) << std::endl;
        out << "#   " << p.x << " " << p.y << " " << p.w;
        return out;
    }
}


namespace GEO {

    CDTBase2d::CDTBase2d() :
        nv_(0),
        ncnstr_(0),
        delaunay_(true),
        exact_incircle_(true) {
    }

    CDTBase2d::~CDTBase2d() {
    }

    void CDTBase2d::clear() {
        nv_ = 0;
        ncnstr_ = 0;
        T_.resize(0);
        Tadj_.resize(0);
        v2T_.resize(0);
        Tflags_.resize(0);
        Tecnstr_.resize(0);
        Tnext_.resize(0);
        Tprev_.resize(0);
    }

    void CDTBase2d::create_enclosing_triangle(
        index_t v0, index_t v1, index_t v2
    ) {
        nv_ = 3;
        v2T_.resize(3);
        geo_debug_assert(v0 <= 3);
        geo_debug_assert(v1 <= 3);
        geo_debug_assert(v2 <= 3);
        index_t t0 = Tnew();
        Tset(t0, v0, v1, v2, index_t(-1), index_t(-1), index_t(-1));
        orient_012_ = orient2d(0,1,2);
    }

    void CDTBase2d::create_enclosing_quad(
        index_t v0, index_t v1, index_t v2, index_t v3
    ) {
        nv_ = 4;
        v2T_.resize(4);
        geo_debug_assert(v0 <= 4);
        geo_debug_assert(v1 <= 4);
        geo_debug_assert(v2 <= 4);
        geo_debug_assert(v3 <= 4);        
        index_t t0 = Tnew();
        index_t t1 = Tnew();        
        Tset(t0, v0, v1, v3, t1, index_t(-1), index_t(-1));
        Tset(t1, v3, v1, v2, index_t(-1), index_t(-1), t0);
        orient_012_ = orient2d(0,1,2);
        geo_debug_assert(is_convex_quad(t0));
        if(incircle(v0,v1,v2,v3) == POSITIVE) {
            swap_edge(t0);
        }
    }
    
    index_t CDTBase2d::insert(index_t v, index_t hint) {
        bool keep_duplicates = false;
        if(v == nv()) {
            v2T_.push_back(index_t(-1));
            ++nv_;
        } else {
            // We are inserting a vertex in the middle of the
            // list, which means we are doing batch-insertion,
            // then we will not discard duplicates.
            keep_duplicates = true;
            geo_debug_assert(v < nv_);
        }
        
        // Phase 1: find triangle that contains vertex i
        Sign o[3];
        index_t t = locate(v,hint,o);
        int nb_z = (o[0] == ZERO) + (o[1] == ZERO) + (o[2] == ZERO);
        geo_debug_assert(nb_z != 3);

        // Duplicated vertex
        if(nb_z == 2) {
            CDT_LOG("duplicated vertex");
            v = (o[0] != ZERO) ? Tv(t,0) :
                (o[1] != ZERO) ? Tv(t,1) :
                                 Tv(t,2) ;
            if(!keep_duplicates) {
                v2T_.pop_back();
                --nv_;
            }
            return v;
        }

        // Stack of triangle edges to examine for flipping. Ignored in
        // non-Delaunay mode (ignored if !delaunay_)
        // Note: it is always edge 0 that we examine, since new
        // triangles are always created with v as vertex 0.
        DList S(*this);
        if(delaunay_) {
            S.initialize(DLIST_S_ID);
        }

        // Phase 2: split triangle
        // Particular case: v is on edge
        if(nb_z == 1) {
            CDT_LOG("vertex on edge");            
            index_t le = (o[0] == ZERO) ? 0 :
                         (o[1] == ZERO) ? 1 :
                          2 ;
            insert_vertex_in_edge(v,t,le,S);
        } else {
            insert_vertex_in_triangle(v,t,S);
        }

        // Phase 3: recursively restore Delaunay conditions for the neighbors
        // of the new vertex
        if(delaunay_) {
            Delaunayize_vertex_neighbors(v,S);
        }

        return v;
    }

    
    void CDTBase2d::insert_constraint(index_t i, index_t j) {
        CDT_LOG("insert constraint: " << i << "-" << j);
        debug_check_consistency();
        ++ncnstr_;

        // Index of first vertex coming from constraints intersection
        // (keep track of it to re-Delaunayize their neighborhoods).
        index_t first_v_isect = nv_;

#ifndef CDT_NAIVE        
        DList Q(*this, DLIST_Q_ID); // Queue of edges to constrain
        DList N(*this); // New edges to re-Delaunayize (ignored if !delaunay_)
        if(delaunay_) {
            N.initialize(DLIST_N_ID);
        }
        while(i != j) {
            
            // Step 1: find all the edges that have an intersection 
            // with the constraint [i,j], enqueue them in Q.
            // Stop at vertex on constraint or constraint intersection
            // if any (returned in k)
            index_t k = find_intersected_edges(i,j,Q);
            
            // Step 2: constrain edges
            constrain_edges(i,k,Q,N);
            
            // Step 3: restore Delaunay condition
            if(delaunay_) {
                Delaunayize_new_edges(N);
            }
            
            i = k;
        }
#else
        DList Q(*this, DLIST_Q_ID); // Queue of edges to constrain
        vector<Edge> N; // New edges to re-Delaunayize
        while(i != j) {
            index_t k = find_intersected_edges(i,j,Q);
            constrain_edges_naive(i,k,Q,N);
            debug_check_combinatorics();
            if(delaunay_) {
                Delaunayize_new_edges_naive(N);
            }
            debug_check_combinatorics();            
            i = k;
        }
#endif        

        debug_check_combinatorics();
        
        if(!delaunay_) {
            return;
        }

        // Delaunayize triangles around vertices coming from
        // constraint intersections
        DList S(*this, DLIST_S_ID);        
        for(index_t v=first_v_isect; v<nv(); ++v) {
            // We cannot use for_each_triangle_around_vertex()
            // because we need to Trot() t during traveral,
            // to have v has t's vertex 0
            // But the good news is that v is never on the border,
            // (because it comes from an edge *intersection*),
            // hence traversal is easier.
            index_t t0 = vT(v); // Need to store it, because we Trot()
            index_t t = t0;
            do {
                index_t lv = Tv_find(t,v);
                Trot(t,lv);
                geo_debug_assert(Tv(t,0) == v);
                S.push_back(t);
                t = Tadj(t, 1);
                geo_debug_assert(t != index_t(-1));
            } while(t != t0);            
            Delaunayize_vertex_neighbors(v,S);
        }

        debug_check_consistency();        
    }
    
    void CDTBase2d::Tset_edge_cnstr_with_neighbor(
        index_t t, index_t le, index_t cnstr_id 
    ) {
        geo_debug_assert(t < nT());
        geo_debug_assert(le < 3);
        Tset_edge_cnstr(t, le, cnstr_id);
        index_t t2 = Tadj(t,le);
        if(t2 != index_t(-1)) {
            index_t le2 = Tadj_find(t2,t);
            Tset_edge_cnstr(t2,le2,cnstr_id);
        }
    }
    
    
    /**
     * \brief Used by the implementation of find_intersected_edges()
     * \details During traversal of a constrained edge [i,j], we can be on
     *  a vertex (then v != index_t) or on a triangle (then t != index_t).
     *  We also keep track of the previous vertex (prev_v) and previous
     *  triangle(prev_t) in order to make sure we do not go backwards.
     */
    struct CDT2d_ConstraintWalker {
        /**
         * \brief ConstraintWalker constructor
         * \param[in] i_in , j_in extremities of the constrained edge
         */
        CDT2d_ConstraintWalker(index_t i_in, index_t j_in) :
            i(i_in), j(j_in),
            t_prev(index_t(-1)), v_prev(index_t(-1)),
            t(index_t(-1)), v(i_in),
            v_cnstr(index_t(-1))
            {
            }
        index_t i, j;
        index_t t_prev, v_prev;
        index_t t, v;
        index_t v_cnstr;
    };
    
    index_t CDTBase2d::find_intersected_edges(index_t i, index_t j, DList& Q) {
        CDT_LOG("Find intersected edges: " << i << "-" << j);
        CDT2d_ConstraintWalker W(i,j);
        // Stop at the first encountered vertex or constraint intersection. 
        while(W.v == i || W.v == index_t(-1)) {
            CDT_LOG(
                "   t=" << int(W.t) << " v=" << int(W.v) << "   "
                "t_prev=" << int(W.t_prev) << " v_prev=" << int(W.v_prev)
                << "   "
            );
            if(W.v != index_t(-1)) {
                walk_constraint_v(W);
            } else {
                walk_constraint_t(W,Q);
            }
        }
        return W.v;
    }    

    // The two functions below are more complicated than I wished, but is
    // simpler than it looks like. There are two main different cases:
    //
    // - walk_constraint_v(): we are on a vertex.
    //   traverse all the triangles around v and find the one that
    //   has an intersection. For instance, when we start from vertex i,
    //   and also when the previous step encountered a vertex exactly on
    //   the constrained segment. It is the annoying case where one has
    //   to traverse the triangles incident to v (using the function
    //   for_each_T_around_v() that takes a lambda).
    //
    // - walk_constraint_t(): we are on an edge intersection.
    //   propagate to the neighbor of t accross the intersected edge.
    //   It is the "generic" case, simpler (the next triangle is
    //   determined by the edge of t that is intersected).
    //
    // There are three things that makes things slightly
    // more complicated:
    // - each case has two sub-cases, depending on whether the next
    //   intersection is an existing vertex.
    // - if an existing edge is embedded in the constraint, one needs
    //   to flag that edge as a constraint.
    // - we need to test whether we are arrived at vertex j
    
    void CDTBase2d::walk_constraint_v(CDT2d_ConstraintWalker& W) {
        geo_debug_assert(W.v != index_t(-1));        
        geo_debug_assert(W.t == index_t(-1));
        
        index_t t_next = index_t(-1);
        index_t v_next = index_t(-1);

        for_each_T_around_v(
            W.v, [&](index_t t_around_v, index_t le) {
                if(t_around_v == W.t_prev) { // Don't go backwards !
                    return false;
                }
                index_t v1 = Tv(t_around_v, (le + 1)%3);
                index_t v2 = Tv(t_around_v, (le + 2)%3);
                if(v1 == W.j || v2 == W.j) { // Are we arrived at j ? 
                    v_next = W.j;
                    // Edge is flagged as constraint here, because
                    // it will not be seen by constraint enforcement.
                    index_t le_cnstr_edge = (v1 == W.j) ? (le+2)%3 : (le+1)%3;
                    Tset_edge_cnstr_with_neighbor(
                        t_around_v, le_cnstr_edge, ncnstr_-1
                    );
                    CDT_LOG(
                        " During cnstr " << W.i << "-" << W.j << ": " 
                        << " Constrained edge "
                        << Tv(t_around_v, (le_cnstr_edge+1)%3) << "-"
                        << Tv(t_around_v, (le_cnstr_edge+2)%3)
                    );
                    return true;
                }
                Sign o1 = orient2d(W.i,W.j,v1);
                Sign o2 = orient2d(W.i,W.j,v2);                        
                Sign o3 = orient2d(v1,v2,W.j);
                Sign o4 = orient_012_; // equivalent to orient2d(v1,v2,i)
                if(o1*o2 < 0 && o3*o4 < 0) {
                    Trot(t_around_v,le); // so that le becomes edge 0
                    t_next = t_around_v; // will be added to Q during next round
                    return true;
                } else {
                    // Special case: v1 or v2 is exactly on [i,j]
                    // Edge is flagged as constraint here, because
                    // it will not be seen by constraint enforcement.
                    geo_debug_assert(o1 != ZERO || o2 != ZERO);
                    if(o1 == ZERO && o3*o4 < 0 && v1 != W.v_prev) {
                        v_next = v1;
                        Tset_edge_cnstr_with_neighbor(
                            t_around_v, (le + 2)%3, ncnstr_-1
                        );
                        return true;
                    } else if(o2 == ZERO && o3*o4 < 0 && v2 != W.v_prev) {
                        v_next = v2;
                        Tset_edge_cnstr_with_neighbor(
                            t_around_v, (le + 1)%3, ncnstr_-1
                        );                                
                        return true;
                    }
                }
                return false;
            }
        ); // End of for_each_T_around_v() loop
        W.t_prev = W.t;
        W.v_prev = W.v;
        W.t = t_next;
        W.v = v_next;
    }

    void CDTBase2d::walk_constraint_t(CDT2d_ConstraintWalker& W, DList& Q) {
        geo_debug_assert(W.v == index_t(-1));
        geo_debug_assert(W.t != index_t(-1));
        
        index_t v_next = index_t(-1);
        index_t t_next = index_t(-1);
        
        if(Tv(W.t,0) == W.j || Tv(W.t,1) == W.j || Tv(W.t,2) == W.j) {
            v_next = W.j; // Are we arrived at j ? 
        } else {
            // Test the three edges of the triangle
            for(index_t le = 0; le<3; ++le) {
                if(Tadj(W.t,le) == W.t_prev) { // Do not go backwards !
                    continue;
                }
                // Test whether [v1,v2] intersects the support line of (i,j).
                // No need to test the *segment* [i,j]: we know the line enters
                // the triangle (it is how we came here), and we know it leaves
                // it, else j would have been one of the triangle's vertices.
                index_t v1 = Tv(W.t, (le + 1)%3);
                index_t v2 = Tv(W.t, (le + 2)%3);
                Sign o1 = orient2d(W.i,W.j,v1);
                Sign o2 = orient2d(W.i,W.j,v2);                        
                if(o1*o2 < 0) {
                    // [v1,v2] has a frank intersection with [i,j]
                    Trot(W.t,le); // So that edge 0 is intersected edge
                    if(Tedge_is_constrained(W.t,0)) {
                        CDT_LOG("   Cnstr isect with:" << v1 << "-" << v2);
                        v_next = create_intersection(
                            ncnstr()-1, W.i, W.j,
                            Tedge_cnstr(W.t,0), v1, v2
                        );
                        insert_vertex_in_edge(v_next,W.t,0);
                        // Mark new edge as constraint if walker was previously
                        // on a vertex.
                        if(W.v_prev != index_t(-1)) {
                            Tset_edge_cnstr_with_neighbor(W.t,2,ncnstr_-1);
                        }
                    } else {
                        CDT_LOG("   Isect: t=" << W.t <<" E=" << v1 <<"-"<< v2);
                        Q.push_back(W.t);
                        t_next = Tadj(W.t,0);
                    }
                    break;
                } else { // Special case: v1 or v2 is exactly on [i,j]
                    geo_debug_assert(o1 != ZERO || o2 != ZERO);
                    if(o1 == ZERO) {
                        v_next = v1;
                        break;
                    } else if(o2 == ZERO) {
                        v_next = v2;
                        break;
                    }
                }
            }
        }
        W.t_prev = W.t;
        W.v_prev = W.v;
        W.t = t_next;
        W.v = v_next;
    }
    
    void CDTBase2d::constrain_edges(index_t i, index_t j, DList& Q, DList& N) {

#ifdef CDT_DEBUG
        // The function find_edge_intersections() is super complicated,
        // so in debug mode I make sure it did its job correctly (by testing
        // *all* edge intersections).
        check_edge_intersections(i,j,Q);
#endif
        // Called each time edge le of triangle t has no isect with cnstr,
        // (then it is a "new edge")
        auto new_edge = [&](index_t t,index_t le) {
            Trot(t,le);
            if(
                (Tv(t,1) == i && Tv(t,2) == j) ||
                (Tv(t,1) == j && Tv(t,2) == i)
            ) {
                // Set constraint flag if the new edge is the constrained edge
                Tset_edge_cnstr_with_neighbor(t,0,ncnstr_-1);
            } else {
                // Memorize new edge as "to be Delaunayized"
                if(N.initialized()) {
                    N.push_back(t);
                }
            }
        };

        // Called each time edge le of triangle t still has an isect with cnstr
        // (then it is queued again)
        auto isect_edge = [&](index_t t, index_t le) {
            Trot(t,le);
            Q.push_front(t);
        };

        while(!Q.empty()) {
            index_t t1 = Q.pop_back();
            if(!is_convex_quad(t1)) {
                // Sanity check: if the only remaining edge to flip does
                // not form a convex quad, it means we are going to
                // flip forever ! (shoud not happen)
                geo_assert(!Q.empty());
                Q.push_front(t1);
            } else {
                index_t t2 = Tadj(t1,0);
                bool no_isect  = !Q.contains(t2); // O(1), Q uses flags
                index_t v0     = Tv(t1,0);
                bool t2v0_t1v2 = (Q.contains(t2) && Tv(t2,0) == Tv(t1,2));
                bool t2v0_t1v1 = (Q.contains(t2) && Tv(t2,0) == Tv(t1,1));
                geo_argused(t2v0_t1v1);

                if(no_isect) {
                    swap_edge(t1);
                    geo_debug_assert(!segment_edge_intersect(i,j,t1,2));
                    new_edge(t1,2);
                } else {
                    // See comment at beginning of file
                    // (a small variation in Sloan's
                    // method that makes better use of the combinatorics)
                    Sign o = Sign(orient2d(i,j,v0) * orient_012_);
                    if(t2v0_t1v2) {
                        swap_edge(t1,false); // "new t1 on top"
                        if(o >= 0) {
                            geo_debug_assert(!segment_edge_intersect(i,j,t1,2));
                            geo_debug_assert( segment_edge_intersect(i,j,t2,0));
                            new_edge(t1,2);
                        } else {
                            geo_debug_assert( segment_edge_intersect(i,j,t1,2));
                            geo_debug_assert( segment_edge_intersect(i,j,t2,0));
                            isect_edge(t1,2);
                        }
                    } else {
                        geo_debug_assert(t2v0_t1v1);
                        swap_edge(t1,true); // "new t1 on bottom"    
                        if(o > 0) {
                            geo_debug_assert( segment_edge_intersect(i,j,t1,1));
                            geo_debug_assert( segment_edge_intersect(i,j,t2,0));
                            isect_edge(t1,1); 
                        } else {
                            geo_debug_assert(!segment_edge_intersect(i,j,t1,1));
                            geo_debug_assert( segment_edge_intersect(i,j,t2,0));
                            new_edge(t1,1);
                        }
                    }
                }
            }
        }
    }

    void CDTBase2d::Delaunayize_vertex_neighbors(index_t v, DList& S) {
        while(!S.empty()) {
            index_t t1 = S.pop_back();
            geo_debug_assert(Tv(t1,0) == v);
            if(Tedge_is_constrained(t1,0)) {
                continue;
            }
            index_t t2 = Tadj(t1,0);
            if(t2 == index_t(-1)) {
                continue;
            }
            if(!exact_incircle_ && !is_convex_quad(t1)) {
                continue;
            }
            index_t v1 = Tv(t2,0);
            index_t v2 = Tv(t2,1);
            index_t v3 = Tv(t2,2);
            if(incircle(v1,v2,v3,v) == POSITIVE && incircle(v1,v2,v,v3) == NEGATIVE) { // HERE
                swap_edge(t1);
                S.push_back(t1);
                S.push_back(t2);                    
            }
        }
    }
    
    void CDTBase2d::Delaunayize_new_edges(DList& N) {
        bool swap_occured = true;
        while(swap_occured) {
            swap_occured = false;
            for(index_t t1 = N.front(); t1 != index_t(-1); t1 = N.next(t1)) {
                if(Tedge_is_constrained(t1,0)) {
                    continue;
                }
                index_t v1 = Tv(t1,1);
                index_t v2 = Tv(t1,2);
                index_t v0 = Tv(t1,0);
                index_t t2 = Tadj(t1,0);
                if(t2 == index_t(-1)) {
                    continue;
                }
                if(!exact_incircle_ && !is_convex_quad(t1)) {
                    continue;
                }
                index_t e2 = Tadj_find(t2,t1);
                index_t v3 = Tv(t2,e2);
                if(incircle(v0,v1,v2,v3) == POSITIVE && incircle(v0,v1,v3,v2) == NEGATIVE) { // HERE
                    // t2 may also encode a new edge, we need to preserve it,
                    // by chosing the right swap:
                    if(Tv(t2,0) == Tv(t1,1)) {
                        swap_edge(t1, true); // t2 on top
                        Trot(t1,1);
                    } else {
                        swap_edge(t1, false); // t1 on top
                        Trot(t1,2);
                    }
                    swap_occured = true;
                } 
            }
        }
        N.clear();
    }

    index_t CDTBase2d::locate(index_t v, index_t hint, Sign* o) const {
        Sign o_local[3];
        if(o == nullptr) {
            o = o_local;
        }

        // Efficient locate, "walking the triangulation"
        index_t nb_traversed_t = 0;
        index_t t_pred = nT()+1; // Needs to be different from index_t(-1)
        index_t t = (hint == index_t(-1)) ?
                     index_t(random()) % nT() :
                     hint ;
    still_walking:
        {
            ++nb_traversed_t;

            // Infinite loop are not supposed to happen, but
            // let us detect them, just in case...
            geo_debug_assert(nb_traversed_t <= 2*nT());
            
            // You will land here if we try to locate a point outside
            // the boundary
            bool point_outside_boundary = (t == index_t(-1));
            geo_argused(point_outside_boundary);
            geo_assert(!point_outside_boundary);
             
            index_t tv[3];
            tv[0] = Tv(t,0);
            tv[1] = Tv(t,1);
            tv[2] = Tv(t,2);
            
            // Start from a random edge
            index_t e0 = index_t(random()) % 3;
            for(index_t de = 0; de < 3; ++de) {
                index_t le = (e0 + de) % 3;
                
                index_t t_next = Tadj(t,le);

                //   If the candidate next triangle is the
                // one we came from, then we know already that
                // the orientation is positive, thus we examine
                // the next candidate (or exit the loop if they
                // are exhausted).
                //
                // (here is why intial value of t_pred needs to be
                // different from index_t(-1))
                if(t_next == t_pred) {
                    o[le] = POSITIVE;
                    continue ; 
                }

                // To test the orientation of p w.r.t. the facet f of
                // t, we replace vertex number f with p in t (same
                // convention as in CGAL).
                index_t v_bkp = tv[le];
                tv[le] = v;
                o[le] = Sign(orient_012_ * orient2d(tv[0], tv[1], tv[2]));
                
                // If the orientation is not negative, then we cannot
                // walk towards t_next, and examine the next candidate
                // (or exit the loop if they are exhausted).
                if(o[le] != NEGATIVE) {
                    tv[le] = v_bkp;
                    continue;
                }

                // If we reach this point, then t_next is a valid
                // successor, thus we are still walking.
                t_pred = t;
                t = t_next;
                goto still_walking;
            }
        }

        return t;
    }
    
    void CDTBase2d::remove_marked_triangles() {
        // Step 1: compute old2new map
        // (use Tnext_'s storage, that we do not need now)
        vector<index_t>& old2new = Tnext_;
        index_t cur_t_new = 0;
        for(index_t t=0; t<nT(); ++t) {
            if(Tflag_is_set(t,T_MARKED_FLAG)) {
                old2new[t] = index_t(-1);
            } else {
                old2new[t] = cur_t_new;
                ++cur_t_new;
            }
        }
        index_t nT_new = cur_t_new;

        // Step 2: translate adjacency and move triangles
        for(index_t t=0; t<nT(); ++t) {
            index_t t_new = old2new[t];
            if(t_new == index_t(-1)) {
                continue;
            }
            index_t adj0 = Tadj(t,0);
            if(adj0 != index_t(-1)) {
                adj0 = old2new[adj0];
            }
            index_t adj1 = Tadj(t,1);
            if(adj1 != index_t(-1)) {
                adj1 = old2new[adj1];
            }
            index_t adj2 = Tadj(t,2);
            if(adj2 != index_t(-1)) {
                adj2 = old2new[adj2];
            }
            Tset(
                t_new,
                Tv(t,0), Tv(t,1), Tv(t,2),
                adj0, adj1, adj2,
                Tedge_cnstr(t,0), Tedge_cnstr(t,1), Tedge_cnstr(t,2)
            );
            Tflags_[t_new] = 0;
        }

        // Step 3: resize arrays
        T_.resize(3*nT_new);
        Tadj_.resize(3*nT_new);
        Tflags_.resize(nT_new);
        Tecnstr_.resize(3*nT_new);
        Tnext_.resize(nT_new);
        Tprev_.resize(nT_new);

        // Step 4: fix v2T_
        for(index_t t=0; t<nT(); ++t) {
            v2T_[Tv(t,0)] = t;
            v2T_[Tv(t,1)] = t;
            v2T_[Tv(t,2)] = t;            
        }
    }
    
    
    /***************** Triangulation surgery (boring code ahead) *********/
    
    void CDTBase2d::insert_vertex_in_edge(
        index_t v, index_t t, index_t le1, DList& S
    ) {
        index_t cnstr = Tedge_cnstr(t,le1);
        index_t t1 = t;
        index_t t2 = Tadj(t1,le1);
        index_t v1 = Tv(t1,le1);
        index_t v2 = Tv(t1,(le1+1)%3);
        index_t v3 = Tv(t1,(le1+2)%3);
        index_t t1_adj2 = Tadj(t1,(le1+1)%3);
        index_t t1_adj3 = Tadj(t1,(le1+2)%3);
        if(t2 != index_t(-1)) {
            // New vertex is on an edge of t1 and t1 has a neighbor
            // accross that edge. Discard the two triangles t1 and t2
            // adjacent to the edge, and create four new triangles
            // (t1 and t2 are recycled).
            index_t le2 = Tadj_find(t2,t1);
            geo_debug_assert(Tv(t2, (le2+1)%3) == v3);
            geo_debug_assert(Tv(t2, (le2+2)%3) == v2);
            index_t v4 = Tv(t2,le2);
            index_t t2_adj2 = Tadj(t2,(le2+1)%3);
            index_t t2_adj3 = Tadj(t2,(le2+2)%3);
            index_t t3 = Tnew();
            index_t t4 = Tnew();
            Tset(t1,v,v1,v2,t1_adj3,t2,t4);
            Tset(t2,v,v2,v4,t2_adj2,t3,t1);
            Tset(t3,v,v4,v3,t2_adj3,t4,t2);
            Tset(t4,v,v3,v1,t1_adj2,t1,t3);
            Tadj_back_connect(t1,0,t1);
            Tadj_back_connect(t2,0,t2);
            Tadj_back_connect(t3,0,t2);
            Tadj_back_connect(t4,0,t1);
            Tset_edge_cnstr(t1,1,cnstr);
            Tset_edge_cnstr(t2,2,cnstr);
            Tset_edge_cnstr(t3,1,cnstr);
            Tset_edge_cnstr(t4,2,cnstr);
            if(S.initialized()) {
                S.push_back(t1);
                S.push_back(t2);
                S.push_back(t3);
                S.push_back(t4);                
            }
        } else {
            // New vertex is on an edge of t1 and t1 has no neighbor
            // accross that edge. Discard t1 and replace it with two
            // new triangles (recycle t1).
            t2 = Tnew();
            Tset(t1,v,v1,v2,t1_adj3,index_t(-1),t2);
            Tset(t2,v,v3,v1,t1_adj2,t1,index_t(-1));
            Tadj_back_connect(t1,0,t1);
            Tadj_back_connect(t2,0,t1);            
            Tset_edge_cnstr(t1,1,cnstr);
            Tset_edge_cnstr(t2,2,cnstr);
            if(S.initialized()) {
                S.push_back(t1);
                S.push_back(t2);
            }
        }
    }

    void CDTBase2d::insert_vertex_in_triangle(index_t v, index_t t, DList& S) {
        // New vertex is in t1. Discard t1 and replace it with three
        // new triangles (recycle t1).
        index_t t1 = t;
        index_t v1 = Tv(t1,0);
        index_t v2 = Tv(t1,1);
        index_t v3 = Tv(t1,2);
        index_t adj1 = Tadj(t1,0);
        index_t adj2 = Tadj(t1,1);
        index_t adj3 = Tadj(t1,2);
        index_t t2 = Tnew();
        index_t t3 = Tnew();
        Tset(t1,v,v2,v3,adj1,t2,t3);
        Tset(t2,v,v3,v1,adj2,t3,t1);
        Tset(t3,v,v1,v2,adj3,t1,t2);
        Tadj_back_connect(t1,0,t1);
        Tadj_back_connect(t2,0,t1);
        Tadj_back_connect(t3,0,t1);
        if(S.initialized()) {
            S.push_back(t1);
            S.push_back(t2);
            S.push_back(t3);            
        }
    }
    
    void CDTBase2d::swap_edge(index_t t1, bool swap_t1_t2) {
        geo_debug_assert(!Tedge_is_constrained(t1,0));
        index_t v1 = Tv(t1,0);
        index_t v2 = Tv(t1,1);
        index_t v3 = Tv(t1,2);                        
        index_t t1_adj2 = Tadj(t1,1);
        index_t t1_adj3 = Tadj(t1,2);
        index_t t2 = Tadj(t1,0);
        index_t le2 = Tadj_find(t2,t1);
        index_t v4 = Tv(t2,le2);
        geo_debug_assert(Tv(t2,(le2+1)%3) == v3);
        geo_debug_assert(Tv(t2,(le2+2)%3) == v2);
        
        debug_Tcheck(t1);
        debug_Tcheck(t2);
        
        index_t t2_adj2 = Tadj(t2,(le2+1)%3);
        index_t t2_adj3 = Tadj(t2,(le2+2)%3);
        if(swap_t1_t2) {
            Tset(t2,v1,v4,v3,t2_adj3,t1_adj2,t1);
            Tset(t1,v1,v2,v4,t2_adj2,t2,t1_adj3);
            Tadj_back_connect(t2,0,t2);
            Tadj_back_connect(t2,1,t1);
            Tadj_back_connect(t1,0,t2);
            Tadj_back_connect(t1,2,t1);
        } else {
            Tset(t1,v1,v4,v3,t2_adj3,t1_adj2,t2);
            Tset(t2,v1,v2,v4,t2_adj2,t1,t1_adj3);
            Tadj_back_connect(t1,0,t2);
            Tadj_back_connect(t1,1,t1);
            Tadj_back_connect(t2,0,t2);
            Tadj_back_connect(t2,2,t1);
        }

        debug_Tcheck(t1);
        debug_Tcheck(t2);
    }

    /***************** Geometry ***********************/
    
    bool CDTBase2d::is_convex_quad(index_t t) const {
        index_t v1 = Tv(t,0);
        index_t v2 = Tv(t,1);
        index_t v3 = Tv(t,2);        
        index_t t2 = Tadj(t,0);
        index_t le2 = Tadj_find(t2,t);
        index_t v4 = Tv(t2,le2);
        // t and Tadj(t,0) have the correct orientation,
        // so one just needs to check the orientation of
        // the two triangles that would be generated by
        // an edge flip.
        return
            orient2d(v1,v4,v3) == orient_012_ &&
            orient2d(v4,v1,v2) == orient_012_ ;
    }

    /**  Debugging ******************************************************/

    bool CDTBase2d::Tedge_is_Delaunay(index_t t1, index_t le1) const {
        if(Tedge_is_constrained(t1,le1)) {
            return true;
        }
        index_t t2 = Tadj(t1,le1);
        if(t2 == index_t(-1)) {
            return true;
        }
        index_t le2 = Tadj_find(t2,t1);
        index_t v1 = Tv(t1,le1);
        index_t v2 = Tv(t1,(le1+1)%3);
        index_t v3 = Tv(t1,(le1+2)%3);
        index_t v4 = Tv(t2,le2);
        return incircle(v1,v2,v3,v4) <= 0;
    }

    void CDTBase2d::check_edge_intersections(
        index_t v1, index_t v2, const DList& Q
    ) {
        std::set<Edge> I;
        auto make_edge = [](index_t w1, index_t w2)->Edge {
            return std::make_pair(std::min(w1,w2), std::max(w1,w2));
        };
        for(index_t t=Q.front(); t!=index_t(-1); t = Q.next(t)) {
            geo_assert(segment_edge_intersect(v1,v2,t,0));
            I.insert(make_edge(Tv(t,1), Tv(t,2)));
        }
        for(index_t t=0; t<nT(); ++t) {
            for(index_t le=0; le<3; ++le) {
                if(segment_edge_intersect(v1,v2,t,le)) {
                    index_t w1 = Tv(t,(le+1)%3);
                    index_t w2 = Tv(t,(le+2)%3);
                    geo_argused(w1);
                    geo_argused(w2);
                    geo_assert(I.find(make_edge(w1,w2)) != I.end());
                }
            }
        }
    }

    /*** Naive versions of algorithms, for reference / debugging if need be ***/
    
    index_t CDTBase2d::locate_naive(index_t v, index_t hint, Sign* o) const {
        geo_argused(hint);
        Sign o_local[3];
        if(o == nullptr) {
            o = o_local;
        }

        for(index_t t=0; t<nT(); ++t) {
            index_t i = Tv(t,0);
            index_t j = Tv(t,1);
            index_t k = Tv(t,2);
            o[0] = orient2d(v,j,k);
            o[1] = orient2d(v,k,i);
            o[2] = orient2d(v,i,j);
            if(o[0]*o[1] >= 0 && o[1]*o[2] >= 0 && o[2]*o[0] >= 0) {
                return t;
            }
        }
        geo_assert_not_reached;
    }
    
    void CDTBase2d::Delaunayize_new_edges_naive(vector<Edge>& N) {
        for(Edge E: N) {
            index_t v1 = std::min(E.first,E.second);
            index_t v2 = std::max(E.first,E.second);
            if(v2 < v1) {
                std::swap(v1,v2);
            }
            CDT_LOG("new edge: " << v1 << " " << v2);
        }
        bool swap_occured = true;
        while(swap_occured) {
            swap_occured = false;
            for(Edge& E: N) {
                index_t t1 = eT(E);
                if(Tedge_is_constrained(t1,0)) {
                    continue;
                }
                index_t v1 = Tv(t1,1);
                index_t v2 = Tv(t1,2);
                index_t v0 = Tv(t1,0);
                index_t t2 = Tadj(t1,0);
                if(t2 == index_t(-1)) {
                    continue;
                }
                index_t e2 = Tadj_find(t2,t1);
                index_t v3 = Tv(t2,e2);
                if(incircle(v0,v1,v2,v3) == POSITIVE) {
                    swap_edge(t1);
                    E = std::make_pair(Tv(t1,0), Tv(t1,1));
                    swap_occured = true;
                } 
            }
        }
        N.resize(0);
    }
    
    void CDTBase2d::constrain_edges_naive(
        index_t i, index_t j, DList& Q_in, vector<Edge>& N
    ) {
        CDT_LOG("Q size=" << Q_in.size());
        
        std::deque<Edge> Q;
        for(index_t t=Q_in.front(); t != index_t(-1); t = Q_in.next(t)) {
            Q.push_back(std::make_pair(Tv(t,1), Tv(t,2)));
        }
        Q_in.clear();

        for(index_t t=0; t<nT(); ++t) {
            geo_debug_assert(!Tis_in_list(t));
        }

        while(Q.size() != 0) {
            Edge E = Q.back();
            Q.pop_back();
            if(!is_convex_quad(eT(E))) {
                if(Q.size() == 0) {
                    CDT_LOG("... infinite iteration");
                    abort();
                }
                Q.push_front(E);
            } else {
                index_t t = eT(E);                                
                swap_edge(t);
                E = std::make_pair(Tv(t,0), Tv(t,1));
                if(segment_segment_intersect(i,j,E.first,E.second)) {
                    Q.push_front(E);
                } else {
                    if(
                        (E.first == i && E.second == j) ||
                        (E.first == j && E.second == i)
                    ) {
                        Tset_edge_cnstr_with_neighbor(eT(E),0,ncnstr_-1);
                    } else {
                        N.push_back(E);
                    }
                }
            }
        }
    }
    
    /********************************************************************/

    CDT2di::CDT2di() {
        // Our incircle() predicate uses double-precision numbers instead
        // of integer exact arithmetics, because when there are intersecting
        // constraints, it overflows int64_t precision.
        exact_incircle_ = false;
    }
    
    CDT2di::~CDT2di() {
    }

    index_t CDT2di::insert(const vec2ih& p, index_t hint) {
        debug_check_consistency();            
        point_.push_back(p);
        simplify(*(point_.rbegin()));
        index_t v = CDTBase2d::insert(point_.size()-1, hint);
        // If inserted point already existed in
        // triangulation, then nv() did not increase
        if(point_.size() > nv()) {
            point_.pop_back();
        }
        debug_check_consistency();                        
        return v;
    }
    
    void CDT2di::insert_constraint(index_t i, index_t j) {
        constraint_.push_back(std::make_pair(i,j));
        CDTBase2d::insert_constraint(i,j);
    }
    
    void CDT2di::clear() {
        CDTBase2d::clear();
        point_.resize(0);
        constraint_.resize(0);
    }
    
    void CDT2di::create_enclosing_triangle(
        const vec2ih& p1, const vec2ih& p2, const vec2ih& p3
    ) {
        geo_assert(nv() == 0);
        geo_assert(nT() == 0);        
        point_.push_back(p1);
        point_.push_back(p2);
        point_.push_back(p3);
        CDTBase2d::create_enclosing_triangle(0,1,2);
    }

    void CDT2di::create_enclosing_quad(
        const vec2ih& p1, const vec2ih& p2, const vec2ih& p3, const vec2ih& p4
    ) {
        geo_assert(nv() == 0);
        geo_assert(nT() == 0);        
        point_.push_back(p1);
        point_.push_back(p2);
        point_.push_back(p3);        
        point_.push_back(p4);
        CDTBase2d::create_enclosing_quad(0,1,2,3);
    }
    
    Sign CDT2di::orient2d(index_t i, index_t j, index_t k) const {
        geo_debug_assert(i < nv());
        geo_debug_assert(j < nv());
        geo_debug_assert(k < nv());


        const vec2ih& p0 = point_[i];
        const vec2ih& p1 = point_[j];
        const vec2ih& p2 = point_[k];
            
        int64_t Delta = det3x3(
            p0.x, p0.y, p0.w,
            p1.x, p1.y, p1.w,
            p2.x, p2.y, p2.w
        );
        return Sign(
            geo_sgn(Delta)*
            geo_sgn(p0.w)*
            geo_sgn(p1.w)*
            geo_sgn(p2.w)
        );
    }

    static inline void approx_vec2ih_diff(
        const vec2ih& U, const vec2ih& V,
        double& x, double& y, double& w
    ) {
        if(U.w == V.w) {
            x = double(U.x - V.x);
            y = double(U.y - V.y);
            w = double(U.w);
        } else {
            x = double(V.w)*double(U.x) - double(U.w)-double(V.x);
            y = double(V.w)*double(U.y) - double(U.w)-double(V.y);
            w = double(U.w)*double(V.w);
        }
    }
    
    Sign CDT2di::incircle(index_t i, index_t j, index_t k, index_t l) const {
        geo_debug_assert(i < nv());
        geo_debug_assert(j < nv());
        geo_debug_assert(k < nv());
        geo_debug_assert(l < nv());

        // incircle() is computed with (approximate) double-precision
        // arithmetics, from the integer homogeneous coordinates,
        // because the exact integer-only version overflows
        // int64_t capacity when there are intersecting constraints.
        // CDT2dBase supports approximate incircle() predicate
        // (then it does additional convexity checks before flipping
        // edges to restore Delaunay-ness). 

        // this version does not work with test4.obj 
        // (to be understood, it is probably wrong)

        /*
        double Ux,Uy,Uw,Vx,Vy,Vw,Wx,Wy,Ww;
        
        approx_vec2ih_diff(point_[i], point_[l], Ux,Uy,Uw);
        approx_vec2ih_diff(point_[j], point_[l], Vx,Vy,Vw);        
        approx_vec2ih_diff(point_[k], point_[l], Wx,Wy,Ww);
        
        double lU = Ux*Ux+Uy*Uy;
        double lV = Vx*Vx+Vy*Vy;
        double lW = Wx*Wx+Wy*Wy;

        return Sign(
            geo_sgn(det3x3(
                Uw*Ux, Uw*Uy, lU,
                Vw*Vx, Vw*Vy, lV,
                Ww*Wx, Ww*Wy, lW
            )) 
        );
        */

        // This version seems to work, except for frame 2357

        double Ux = double(point_[i].x)/double(point_[i].w) -
                    double(point_[l].x)/double(point_[l].w);

        double Uy = double(point_[i].y)/double(point_[i].w) -
                    double(point_[l].y)/double(point_[l].w);

        double Vx = double(point_[j].x)/double(point_[j].w) -
                    double(point_[l].x)/double(point_[l].w);

        double Vy = double(point_[j].y)/double(point_[j].w) -
                    double(point_[l].y)/double(point_[l].w);

        double Wx = double(point_[k].x)/double(point_[k].w) -
                    double(point_[l].x)/double(point_[l].w);

        double Wy = double(point_[k].y)/double(point_[k].w) -
                    double(point_[l].y)/double(point_[l].w);
        
        double lU = Ux*Ux+Uy*Uy;
        double lV = Vx*Vx+Vy*Vy;
        double lW = Wx*Wx+Wy*Wy;

        return Sign(
            geo_sgn(det3x3(
                Ux, Uy, lU,
                Vx, Vy, lV,
                Wx, Wy, lW
            )) 
        );

        
        // This is the exact commented-out version, that
        // overflows int64_t capacity when there are
        // intersecting constraints (so we use the approximate
        // double-precision version above).
        /*
        vec2ih U = point_[i]-point_[l];
        vec2ih V = point_[j]-point_[l];
        vec2ih W = point_[k]-point_[l];
        int lU = U.x*U.x+U.y*U.y;
        int lV = V.x*V.x+V.y*V.y;
        int lW = W.x*W.x+W.y*W.y;
        return Sign(
            geo_sgn(det3x3(
                U.w*U.x, U.w*U.y, lU,
                V.w*V.x, V.w*V.y, lV,
                W.w*W.x, W.w*W.y, lW
            )) 
        );
        */
    }

    index_t CDT2di::create_intersection(
        index_t E1, index_t i, index_t j,
        index_t E2, index_t k, index_t l
    ) {
        geo_argused(E1);
        geo_argused(E2);        
        geo_debug_assert(i < nv());
        geo_debug_assert(j < nv());
        geo_debug_assert(k < nv());
        geo_debug_assert(l < nv());
        geo_debug_assert(E1 < ncnstr());
        geo_debug_assert(E2 < ncnstr());
        
        i = constraint_[E1].first;
        j = constraint_[E1].second;
        k = constraint_[E2].first;
        l = constraint_[E2].second;

        const vec2ih& p1 = point_[i];
        const vec2ih& p2 = point_[j];
        const vec2ih& q1 = point_[k];
        const vec2ih& q2 = point_[l];        

        vec2ih U = p2-p1;
        vec2ih V = q2-q1;
        vec2ih D = q1-p1;
        
        int64_t t_num   =  det2x2(D.x,D.y,V.x,V.y)*U.w;
        int64_t t_denom =  det2x2(U.x,U.y,V.x,V.y)*D.w;

        vec2ih I;
        if(p1.w == p2.w) {
            I = vec2ih(
                p1.x * (t_denom - t_num) + p2.x * t_num,
                p1.y * (t_denom - t_num) + p2.y * t_num,
                t_denom * p1.w
            );
            simplify(I);
        } else {
            // Happens when there is an intersection between constraints
            // where one of the constraints has a previous constraint
            // intersection as a vertex. Since this overflows int64_t
            // capacity in most cases, we abort.
            geo_assert_not_reached;
            I = vec2ih(
                p1.x * p2.w * (t_denom - t_num) + p2.x * p1.w * t_num,
                p1.y * p2.w * (t_denom - t_num) + p2.y * p1.w * t_num,
                t_denom * p1.w * p2.w
            );
            simplify(I);
        }
        
        point_.push_back(I);
        v2T_.push_back(index_t(-1));
        index_t v = nv_;
        ++nv_;
        return v;
    }

    void CDT2di::save(const std::string& filename) const {
        std::ofstream out(filename);
        for(index_t v=0; v<nv(); ++v) {
            double x =  double(point(v).x)/double(point(v).w);
            double y = -double(point(v).y)/double(point(v).w);
            out << "v " << x << " " << y << " 0" << std::endl;
        }
        for(index_t t=0; t<nT(); ++t) {
            out << "f " << Tv(t,0)+1 << " " << Tv(t,1)+1 << " " << Tv(t,2)+1
                << std::endl;
        }
        for(index_t t=0; t<nT(); ++t) {
            for(index_t le=0; le<3; ++le) {
                if(Tedge_is_constrained(t,le)) {
                    out << "l "
                        << Tv(t, (le+1)%3)+1 << " "
                        << Tv(t, (le+2)%3)+1
                        << std::endl;
                }
            }
        }
    }

    void CDT2di::Tset_edge_cnstr_with_neighbor(
        index_t t, index_t le, index_t cnstr_id 
    ) {
        CDTBase2d::Tset_edge_cnstr_with_neighbor(
            t,le,cnstr_id
        );

        index_t v1 = Tv(t, (le + 1)%3);
        index_t v2 = Tv(t, (le + 2)%3);
        auto K = std::make_pair(std::min(v1,v2), std::max(v1,v2));
        auto it = constraint_count_.find(K);
        if(it == constraint_count_.end()) {
            constraint_count_[K] = 1;
        } else {
            it->second++;
        }
    }
    
}

