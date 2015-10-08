
#include "main.h"

//${CONFIG_BEGIN}
#define CFG_BINARY_FILES *.bin|*.dat
#define CFG_BRL_GAMETARGET_IMPLEMENTED 1
#define CFG_BRL_THREAD_IMPLEMENTED 1
#define CFG_CD 
#define CFG_CONFIG release
#define CFG_CPP_GC_MODE 1
#define CFG_GLFW_SWAP_INTERVAL -1
#define CFG_GLFW_USE_MINGW 1
#define CFG_GLFW_VERSION 3
#define CFG_GLFW_WINDOW_DECORATED 1
#define CFG_GLFW_WINDOW_FLOATING 0
#define CFG_GLFW_WINDOW_FULLSCREEN 0
#define CFG_GLFW_WINDOW_HEIGHT 720
#define CFG_GLFW_WINDOW_RESIZABLE 1
#define CFG_GLFW_WINDOW_SAMPLES 0
#define CFG_GLFW_WINDOW_TITLE Rebound
#define CFG_GLFW_WINDOW_WIDTH 1280
#define CFG_HOST winnt
#define CFG_IMAGE_FILES *.png|*.jpg
#define CFG_LANG cpp
#define CFG_MODPATH 
#define CFG_MOJO_AUTO_SUSPEND_ENABLED 1
#define CFG_MOJO_DRIVER_IMPLEMENTED 1
#define CFG_MOJO_IMAGE_FILTERING_ENABLED 1
#define CFG_MUSIC_FILES *.wav|*.ogg
#define CFG_OPENGL_DEPTH_BUFFER_ENABLED 0
#define CFG_OPENGL_GLES20_ENABLED 0
#define CFG_SAFEMODE 0
#define CFG_SOUND_FILES *.wav|*.ogg
#define CFG_TARGET glfw
#define CFG_TEXT_FILES *.txt|*.xml|*.json
//${CONFIG_END}

//${TRANSCODE_BEGIN}

#include <wctype.h>
#include <locale.h>

// C++ Monkey runtime.
//
// Placed into the public domain 24/02/2011.
// No warranty implied; use at your own risk.

//***** Monkey Types *****

typedef wchar_t Char;
template<class T> class Array;
class String;
class Object;

#if CFG_CPP_DOUBLE_PRECISION_FLOATS
typedef double Float;
#define FLOAT(X) X
#else
typedef float Float;
#define FLOAT(X) X##f
#endif

void dbg_error( const char *p );

#if !_MSC_VER
#define sprintf_s sprintf
#define sscanf_s sscanf
#endif

//***** GC Config *****

#if CFG_CPP_GC_DEBUG
#define DEBUG_GC 1
#else
#define DEBUG_GC 0
#endif

// GC mode:
//
// 0 = disabled
// 1 = Incremental GC every OnWhatever
// 2 = Incremental GC every allocation
//
#ifndef CFG_CPP_GC_MODE
#define CFG_CPP_GC_MODE 1
#endif

//How many bytes alloced to trigger GC
//
#ifndef CFG_CPP_GC_TRIGGER
#define CFG_CPP_GC_TRIGGER 8*1024*1024
#endif

//GC_MODE 2 needs to track locals on a stack - this may need to be bumped if your app uses a LOT of locals, eg: is heavily recursive...
//
#ifndef CFG_CPP_GC_MAX_LOCALS
#define CFG_CPP_GC_MAX_LOCALS 8192
#endif

// ***** GC *****

#if _WIN32

int gc_micros(){
	static int f;
	static LARGE_INTEGER pcf;
	if( !f ){
		if( QueryPerformanceFrequency( &pcf ) && pcf.QuadPart>=1000000L ){
			pcf.QuadPart/=1000000L;
			f=1;
		}else{
			f=-1;
		}
	}
	if( f>0 ){
		LARGE_INTEGER pc;
		if( QueryPerformanceCounter( &pc ) ) return pc.QuadPart/pcf.QuadPart;
		f=-1;
	}
	return 0;// timeGetTime()*1000;
}

#elif __APPLE__

#include <mach/mach_time.h>

int gc_micros(){
	static int f;
	static mach_timebase_info_data_t timeInfo;
	if( !f ){
		mach_timebase_info( &timeInfo );
		timeInfo.denom*=1000L;
		f=1;
	}
	return mach_absolute_time()*timeInfo.numer/timeInfo.denom;
}

#else

int gc_micros(){
	return 0;
}

#endif

#define gc_mark_roots gc_mark

void gc_mark_roots();

struct gc_object;

gc_object *gc_object_alloc( int size );
void gc_object_free( gc_object *p );

struct gc_object{
	gc_object *succ;
	gc_object *pred;
	int flags;
	
	virtual ~gc_object(){
	}
	
	virtual void mark(){
	}
	
	void *operator new( size_t size ){
		return gc_object_alloc( size );
	}
	
	void operator delete( void *p ){
		gc_object_free( (gc_object*)p );
	}
};

gc_object gc_free_list;
gc_object gc_marked_list;
gc_object gc_unmarked_list;
gc_object gc_queued_list;	//doesn't really need to be doubly linked...

int gc_free_bytes;
int gc_marked_bytes;
int gc_alloced_bytes;
int gc_max_alloced_bytes;
int gc_new_bytes;
int gc_markbit=1;

gc_object *gc_cache[8];

void gc_collect_all();
void gc_mark_queued( int n );

#define GC_CLEAR_LIST( LIST ) ((LIST).succ=(LIST).pred=&(LIST))

#define GC_LIST_IS_EMPTY( LIST ) ((LIST).succ==&(LIST))

#define GC_REMOVE_NODE( NODE ){\
(NODE)->pred->succ=(NODE)->succ;\
(NODE)->succ->pred=(NODE)->pred;}

#define GC_INSERT_NODE( NODE,SUCC ){\
(NODE)->pred=(SUCC)->pred;\
(NODE)->succ=(SUCC);\
(SUCC)->pred->succ=(NODE);\
(SUCC)->pred=(NODE);}

void gc_init1(){
	GC_CLEAR_LIST( gc_free_list );
	GC_CLEAR_LIST( gc_marked_list );
	GC_CLEAR_LIST( gc_unmarked_list);
	GC_CLEAR_LIST( gc_queued_list );
}

void gc_init2(){
	gc_mark_roots();
}

#if CFG_CPP_GC_MODE==2

int gc_ctor_nest;
gc_object *gc_locals[CFG_CPP_GC_MAX_LOCALS],**gc_locals_sp=gc_locals;

struct gc_ctor{
	gc_ctor(){ ++gc_ctor_nest; }
	~gc_ctor(){ --gc_ctor_nest; }
};

struct gc_enter{
	gc_object **sp;
	gc_enter():sp(gc_locals_sp){
	}
	~gc_enter(){
#if DEBUG_GC
		static int max_locals;
		int n=gc_locals_sp-gc_locals;
		if( n>max_locals ){
			max_locals=n;
			printf( "max_locals=%i\n",n );
		}
#endif		
		gc_locals_sp=sp;
	}
};

#define GC_CTOR gc_ctor _c;
#define GC_ENTER gc_enter _e;

#else

struct gc_ctor{
};
struct gc_enter{
};

#define GC_CTOR
#define GC_ENTER

#endif

//Can be modified off thread!
static volatile int gc_ext_new_bytes;

#if _MSC_VER
#define atomic_add(P,V) InterlockedExchangeAdd((volatile unsigned int*)P,V)			//(*(P)+=(V))
#define atomic_sub(P,V) InterlockedExchangeSubtract((volatile unsigned int*)P,V)	//(*(P)-=(V))
#else
#define atomic_add(P,V) __sync_fetch_and_add(P,V)
#define atomic_sub(P,V) __sync_fetch_and_sub(P,V)
#endif

//Careful! May be called off thread!
//
void gc_ext_malloced( int size ){
	atomic_add( &gc_ext_new_bytes,size );
}

void gc_object_free( gc_object *p ){

	int size=p->flags & ~7;
	gc_free_bytes-=size;
	
	if( size<64 ){
		p->succ=gc_cache[size>>3];
		gc_cache[size>>3]=p;
	}else{
		free( p );
	}
}

void gc_flush_free( int size ){

	int t=gc_free_bytes-size;
	if( t<0 ) t=0;
	
	while( gc_free_bytes>t ){
	
		gc_object *p=gc_free_list.succ;

		GC_REMOVE_NODE( p );

#if DEBUG_GC
//		printf( "deleting @%p\n",p );fflush( stdout );
//		p->flags|=4;
//		continue;
#endif
		delete p;
	}
}

gc_object *gc_object_alloc( int size ){

	size=(size+7)&~7;
	
	gc_new_bytes+=size;
	
#if CFG_CPP_GC_MODE==2

	if( !gc_ctor_nest ){

#if DEBUG_GC
		int ms=gc_micros();
#endif
		if( gc_new_bytes+gc_ext_new_bytes>(CFG_CPP_GC_TRIGGER) ){
			atomic_sub( &gc_ext_new_bytes,gc_ext_new_bytes );
			gc_collect_all();
			gc_new_bytes=0;
		}else{
			gc_mark_queued( (long long)(gc_new_bytes)*(gc_alloced_bytes-gc_new_bytes)/(CFG_CPP_GC_TRIGGER)+gc_new_bytes );
		}

#if DEBUG_GC
		ms=gc_micros()-ms;
		if( ms>=100 ) {printf( "gc time:%i\n",ms );fflush( stdout );}
#endif
	}
	
#endif

	gc_flush_free( size );

	gc_object *p;
	if( size<64 && (p=gc_cache[size>>3]) ){
		gc_cache[size>>3]=p->succ;
	}else{
		p=(gc_object*)malloc( size );
	}
	
	p->flags=size|gc_markbit;
	GC_INSERT_NODE( p,&gc_unmarked_list );

	gc_alloced_bytes+=size;
	if( gc_alloced_bytes>gc_max_alloced_bytes ) gc_max_alloced_bytes=gc_alloced_bytes;
	
#if CFG_CPP_GC_MODE==2
	*gc_locals_sp++=p;
#endif

	return p;
}

#if DEBUG_GC

template<class T> gc_object *to_gc_object( T *t ){
	gc_object *p=dynamic_cast<gc_object*>(t);
	if( p && (p->flags & 4) ){
		printf( "gc error : object already deleted @%p\n",p );fflush( stdout );
		exit(-1);
	}
	return p;
}

#else

#define to_gc_object(t) dynamic_cast<gc_object*>(t)

#endif

template<class T> T *gc_retain( T *t ){
#if CFG_CPP_GC_MODE==2
	*gc_locals_sp++=to_gc_object( t );
#endif
	return t;
}

template<class T> void gc_mark( T *t ){

	gc_object *p=to_gc_object( t );
	
	if( p && (p->flags & 3)==gc_markbit ){
		p->flags^=1;
		GC_REMOVE_NODE( p );
		GC_INSERT_NODE( p,&gc_marked_list );
		gc_marked_bytes+=(p->flags & ~7);
		p->mark();
	}
}

template<class T> void gc_mark_q( T *t ){

	gc_object *p=to_gc_object( t );
	
	if( p && (p->flags & 3)==gc_markbit ){
		p->flags^=1;
		GC_REMOVE_NODE( p );
		GC_INSERT_NODE( p,&gc_queued_list );
	}
}

template<class T,class V> void gc_assign( T *&lhs,V *rhs ){

	gc_object *p=to_gc_object( rhs );
	
	if( p && (p->flags & 3)==gc_markbit ){
		p->flags^=1;
		GC_REMOVE_NODE( p );
		GC_INSERT_NODE( p,&gc_queued_list );
	}
	lhs=rhs;
}

void gc_mark_locals(){

#if CFG_CPP_GC_MODE==2
	for( gc_object **pp=gc_locals;pp!=gc_locals_sp;++pp ){
		gc_object *p=*pp;
		if( p && (p->flags & 3)==gc_markbit ){
			p->flags^=1;
			GC_REMOVE_NODE( p );
			GC_INSERT_NODE( p,&gc_marked_list );
			gc_marked_bytes+=(p->flags & ~7);
			p->mark();
		}
	}
#endif	
}

void gc_mark_queued( int n ){
	while( gc_marked_bytes<n && !GC_LIST_IS_EMPTY( gc_queued_list ) ){
		gc_object *p=gc_queued_list.succ;
		GC_REMOVE_NODE( p );
		GC_INSERT_NODE( p,&gc_marked_list );
		gc_marked_bytes+=(p->flags & ~7);
		p->mark();
	}
}

void gc_validate_list( gc_object &list,const char *msg ){
	gc_object *node=list.succ;
	while( node ){
		if( node==&list ) return;
		if( !node->pred ) break;
		if( node->pred->succ!=node ) break;
		node=node->succ;
	}
	if( msg ){
		puts( msg );fflush( stdout );
	}
	puts( "LIST ERROR!" );
	exit(-1);
}

//returns reclaimed bytes
void gc_sweep(){

	int reclaimed_bytes=gc_alloced_bytes-gc_marked_bytes;
	
	if( reclaimed_bytes ){
	
		//append unmarked list to end of free list
		gc_object *head=gc_unmarked_list.succ;
		gc_object *tail=gc_unmarked_list.pred;
		gc_object *succ=&gc_free_list;
		gc_object *pred=succ->pred;
		
		head->pred=pred;
		tail->succ=succ;
		pred->succ=head;
		succ->pred=tail;
		
		gc_free_bytes+=reclaimed_bytes;
	}

	//move marked to unmarked.
	if( GC_LIST_IS_EMPTY( gc_marked_list ) ){
		GC_CLEAR_LIST( gc_unmarked_list );
	}else{
		gc_unmarked_list.succ=gc_marked_list.succ;
		gc_unmarked_list.pred=gc_marked_list.pred;
		gc_unmarked_list.succ->pred=gc_unmarked_list.pred->succ=&gc_unmarked_list;
		GC_CLEAR_LIST( gc_marked_list );
	}
	
	//adjust sizes
	gc_alloced_bytes=gc_marked_bytes;
	gc_marked_bytes=0;
	gc_markbit^=1;
}

void gc_collect_all(){

//	puts( "Mark locals" );
	gc_mark_locals();

//	puts( "Marked queued" );
	gc_mark_queued( 0x7fffffff );

//	puts( "Sweep" );
	gc_sweep();

//	puts( "Mark roots" );
	gc_mark_roots();

#if DEBUG_GC
	gc_validate_list( gc_marked_list,"Validating gc_marked_list"  );
	gc_validate_list( gc_unmarked_list,"Validating gc_unmarked_list"  );
	gc_validate_list( gc_free_list,"Validating gc_free_list" );
#endif

}

void gc_collect(){
	
#if CFG_CPP_GC_MODE==1

#if DEBUG_GC
	int ms=gc_micros();
#endif

	if( gc_new_bytes+gc_ext_new_bytes>(CFG_CPP_GC_TRIGGER) ){
		atomic_sub( &gc_ext_new_bytes,gc_ext_new_bytes );
		gc_collect_all();
		gc_new_bytes=0;
	}else{
		gc_mark_queued( (long long)(gc_new_bytes)*(gc_alloced_bytes-gc_new_bytes)/(CFG_CPP_GC_TRIGGER)+gc_new_bytes );
	}

#if DEBUG_GC
	ms=gc_micros()-ms;
//	if( ms>=100 ) {printf( "gc time:%i\n",ms );fflush( stdout );}
	if( ms>10 ) {printf( "gc time:%i\n",ms );fflush( stdout );}
#endif

#endif
}

// ***** Array *****

template<class T> T *t_memcpy( T *dst,const T *src,int n ){
	memcpy( dst,src,n*sizeof(T) );
	return dst+n;
}

template<class T> T *t_memset( T *dst,int val,int n ){
	memset( dst,val,n*sizeof(T) );
	return dst+n;
}

template<class T> int t_memcmp( const T *x,const T *y,int n ){
	return memcmp( x,y,n*sizeof(T) );
}

template<class T> int t_strlen( const T *p ){
	const T *q=p++;
	while( *q++ ){}
	return q-p;
}

template<class T> T *t_create( int n,T *p ){
	t_memset( p,0,n );
	return p+n;
}

template<class T> T *t_create( int n,T *p,const T *q ){
	t_memcpy( p,q,n );
	return p+n;
}

template<class T> void t_destroy( int n,T *p ){
}

template<class T> void gc_mark_elements( int n,T *p ){
}

template<class T> void gc_mark_elements( int n,T **p ){
	for( int i=0;i<n;++i ) gc_mark( p[i] );
}

template<class T> class Array{
public:
	Array():rep( &nullRep ){
	}

	//Uses default...
//	Array( const Array<T> &t )...
	
	Array( int length ):rep( Rep::alloc( length ) ){
		t_create( rep->length,rep->data );
	}
	
	Array( const T *p,int length ):rep( Rep::alloc(length) ){
		t_create( rep->length,rep->data,p );
	}
	
	~Array(){
	}

	//Uses default...
//	Array &operator=( const Array &t )...
	
	int Length()const{ 
		return rep->length; 
	}
	
	T &At( int index ){
		if( index<0 || index>=rep->length ) dbg_error( "Array index out of range" );
		return rep->data[index]; 
	}
	
	const T &At( int index )const{
		if( index<0 || index>=rep->length ) dbg_error( "Array index out of range" );
		return rep->data[index]; 
	}
	
	T &operator[]( int index ){
		return rep->data[index]; 
	}

	const T &operator[]( int index )const{
		return rep->data[index]; 
	}
	
	Array Slice( int from,int term )const{
		int len=rep->length;
		if( from<0 ){ 
			from+=len;
			if( from<0 ) from=0;
		}else if( from>len ){
			from=len;
		}
		if( term<0 ){
			term+=len;
		}else if( term>len ){
			term=len;
		}
		if( term<=from ) return Array();
		return Array( rep->data+from,term-from );
	}

	Array Slice( int from )const{
		return Slice( from,rep->length );
	}
	
	Array Resize( int newlen )const{
		if( newlen<=0 ) return Array();
		int n=rep->length;
		if( newlen<n ) n=newlen;
		Rep *p=Rep::alloc( newlen );
		T *q=p->data;
		q=t_create( n,q,rep->data );
		q=t_create( (newlen-n),q );
		return Array( p );
	}
	
private:
	struct Rep : public gc_object{
		int length;
		T data[0];
		
		Rep():length(0){
			flags=3;
		}
		
		Rep( int length ):length(length){
		}
		
		~Rep(){
			t_destroy( length,data );
		}
		
		void mark(){
			gc_mark_elements( length,data );
		}
		
		static Rep *alloc( int length ){
			if( !length ) return &nullRep;
			void *p=gc_object_alloc( sizeof(Rep)+length*sizeof(T) );
			return ::new(p) Rep( length );
		}
		
	};
	Rep *rep;
	
	static Rep nullRep;
	
	template<class C> friend void gc_mark( Array<C> t );
	template<class C> friend void gc_mark_q( Array<C> t );
	template<class C> friend Array<C> gc_retain( Array<C> t );
	template<class C> friend void gc_assign( Array<C> &lhs,Array<C> rhs );
	template<class C> friend void gc_mark_elements( int n,Array<C> *p );
	
	Array( Rep *rep ):rep(rep){
	}
};

template<class T> typename Array<T>::Rep Array<T>::nullRep;

template<class T> Array<T> *t_create( int n,Array<T> *p ){
	for( int i=0;i<n;++i ) *p++=Array<T>();
	return p;
}

template<class T> Array<T> *t_create( int n,Array<T> *p,const Array<T> *q ){
	for( int i=0;i<n;++i ) *p++=*q++;
	return p;
}

template<class T> void gc_mark( Array<T> t ){
	gc_mark( t.rep );
}

template<class T> void gc_mark_q( Array<T> t ){
	gc_mark_q( t.rep );
}

template<class T> Array<T> gc_retain( Array<T> t ){
#if CFG_CPP_GC_MODE==2
	gc_retain( t.rep );
#endif
	return t;
}

template<class T> void gc_assign( Array<T> &lhs,Array<T> rhs ){
	gc_mark( rhs.rep );
	lhs=rhs;
}

template<class T> void gc_mark_elements( int n,Array<T> *p ){
	for( int i=0;i<n;++i ) gc_mark( p[i].rep );
}
		
// ***** String *****

static const char *_str_load_err;

class String{
public:
	String():rep( &nullRep ){
	}
	
	String( const String &t ):rep( t.rep ){
		rep->retain();
	}

	String( int n ){
		char buf[256];
		sprintf_s( buf,"%i",n );
		rep=Rep::alloc( t_strlen(buf) );
		for( int i=0;i<rep->length;++i ) rep->data[i]=buf[i];
	}
	
	String( Float n ){
		char buf[256];
		
		//would rather use snprintf, but it's doing weird things in MingW.
		//
		sprintf_s( buf,"%.17lg",n );
		//
		char *p;
		for( p=buf;*p;++p ){
			if( *p=='.' || *p=='e' ) break;
		}
		if( !*p ){
			*p++='.';
			*p++='0';
			*p=0;
		}

		rep=Rep::alloc( t_strlen(buf) );
		for( int i=0;i<rep->length;++i ) rep->data[i]=buf[i];
	}

	String( Char ch,int length ):rep( Rep::alloc(length) ){
		for( int i=0;i<length;++i ) rep->data[i]=ch;
	}

	String( const Char *p ):rep( Rep::alloc(t_strlen(p)) ){
		t_memcpy( rep->data,p,rep->length );
	}

	String( const Char *p,int length ):rep( Rep::alloc(length) ){
		t_memcpy( rep->data,p,rep->length );
	}
	
#if __OBJC__	
	String( NSString *nsstr ):rep( Rep::alloc([nsstr length]) ){
		unichar *buf=(unichar*)malloc( rep->length * sizeof(unichar) );
		[nsstr getCharacters:buf range:NSMakeRange(0,rep->length)];
		for( int i=0;i<rep->length;++i ) rep->data[i]=buf[i];
		free( buf );
	}
#endif

#if __cplusplus_winrt
	String( Platform::String ^str ):rep( Rep::alloc(str->Length()) ){
		for( int i=0;i<rep->length;++i ) rep->data[i]=str->Data()[i];
	}
#endif

	~String(){
		rep->release();
	}
	
	template<class C> String( const C *p ):rep( Rep::alloc(t_strlen(p)) ){
		for( int i=0;i<rep->length;++i ) rep->data[i]=p[i];
	}
	
	template<class C> String( const C *p,int length ):rep( Rep::alloc(length) ){
		for( int i=0;i<rep->length;++i ) rep->data[i]=p[i];
	}
	
	String Copy()const{
		Rep *crep=Rep::alloc( rep->length );
		t_memcpy( crep->data,rep->data,rep->length );
		return String( crep );
	}
	
	int Length()const{
		return rep->length;
	}
	
	const Char *Data()const{
		return rep->data;
	}
	
	Char At( int index )const{
		if( index<0 || index>=rep->length ) dbg_error( "Character index out of range" );
		return rep->data[index]; 
	}
	
	Char operator[]( int index )const{
		return rep->data[index];
	}
	
	String &operator=( const String &t ){
		t.rep->retain();
		rep->release();
		rep=t.rep;
		return *this;
	}
	
	String &operator+=( const String &t ){
		return operator=( *this+t );
	}
	
	int Compare( const String &t )const{
		int n=rep->length<t.rep->length ? rep->length : t.rep->length;
		for( int i=0;i<n;++i ){
			if( int q=(int)(rep->data[i])-(int)(t.rep->data[i]) ) return q;
		}
		return rep->length-t.rep->length;
	}
	
	bool operator==( const String &t )const{
		return rep->length==t.rep->length && t_memcmp( rep->data,t.rep->data,rep->length )==0;
	}
	
	bool operator!=( const String &t )const{
		return rep->length!=t.rep->length || t_memcmp( rep->data,t.rep->data,rep->length )!=0;
	}
	
	bool operator<( const String &t )const{
		return Compare( t )<0;
	}
	
	bool operator<=( const String &t )const{
		return Compare( t )<=0;
	}
	
	bool operator>( const String &t )const{
		return Compare( t )>0;
	}
	
	bool operator>=( const String &t )const{
		return Compare( t )>=0;
	}
	
	String operator+( const String &t )const{
		if( !rep->length ) return t;
		if( !t.rep->length ) return *this;
		Rep *p=Rep::alloc( rep->length+t.rep->length );
		Char *q=p->data;
		q=t_memcpy( q,rep->data,rep->length );
		q=t_memcpy( q,t.rep->data,t.rep->length );
		return String( p );
	}
	
	int Find( String find,int start=0 )const{
		if( start<0 ) start=0;
		while( start+find.rep->length<=rep->length ){
			if( !t_memcmp( rep->data+start,find.rep->data,find.rep->length ) ) return start;
			++start;
		}
		return -1;
	}
	
	int FindLast( String find )const{
		int start=rep->length-find.rep->length;
		while( start>=0 ){
			if( !t_memcmp( rep->data+start,find.rep->data,find.rep->length ) ) return start;
			--start;
		}
		return -1;
	}
	
	int FindLast( String find,int start )const{
		if( start>rep->length-find.rep->length ) start=rep->length-find.rep->length;
		while( start>=0 ){
			if( !t_memcmp( rep->data+start,find.rep->data,find.rep->length ) ) return start;
			--start;
		}
		return -1;
	}
	
	String Trim()const{
		int i=0,i2=rep->length;
		while( i<i2 && rep->data[i]<=32 ) ++i;
		while( i2>i && rep->data[i2-1]<=32 ) --i2;
		if( i==0 && i2==rep->length ) return *this;
		return String( rep->data+i,i2-i );
	}

	Array<String> Split( String sep )const{
	
		if( !sep.rep->length ){
			Array<String> bits( rep->length );
			for( int i=0;i<rep->length;++i ){
				bits[i]=String( (Char)(*this)[i],1 );
			}
			return bits;
		}
		
		int i=0,i2,n=1;
		while( (i2=Find( sep,i ))!=-1 ){
			++n;
			i=i2+sep.rep->length;
		}
		Array<String> bits( n );
		if( n==1 ){
			bits[0]=*this;
			return bits;
		}
		i=0;n=0;
		while( (i2=Find( sep,i ))!=-1 ){
			bits[n++]=Slice( i,i2 );
			i=i2+sep.rep->length;
		}
		bits[n]=Slice( i );
		return bits;
	}

	String Join( Array<String> bits )const{
		if( bits.Length()==0 ) return String();
		if( bits.Length()==1 ) return bits[0];
		int newlen=rep->length * (bits.Length()-1);
		for( int i=0;i<bits.Length();++i ){
			newlen+=bits[i].rep->length;
		}
		Rep *p=Rep::alloc( newlen );
		Char *q=p->data;
		q=t_memcpy( q,bits[0].rep->data,bits[0].rep->length );
		for( int i=1;i<bits.Length();++i ){
			q=t_memcpy( q,rep->data,rep->length );
			q=t_memcpy( q,bits[i].rep->data,bits[i].rep->length );
		}
		return String( p );
	}

	String Replace( String find,String repl )const{
		int i=0,i2,newlen=0;
		while( (i2=Find( find,i ))!=-1 ){
			newlen+=(i2-i)+repl.rep->length;
			i=i2+find.rep->length;
		}
		if( !i ) return *this;
		newlen+=rep->length-i;
		Rep *p=Rep::alloc( newlen );
		Char *q=p->data;
		i=0;
		while( (i2=Find( find,i ))!=-1 ){
			q=t_memcpy( q,rep->data+i,i2-i );
			q=t_memcpy( q,repl.rep->data,repl.rep->length );
			i=i2+find.rep->length;
		}
		q=t_memcpy( q,rep->data+i,rep->length-i );
		return String( p );
	}

	String ToLower()const{
		for( int i=0;i<rep->length;++i ){
			Char t=towlower( rep->data[i] );
			if( t==rep->data[i] ) continue;
			Rep *p=Rep::alloc( rep->length );
			Char *q=p->data;
			t_memcpy( q,rep->data,i );
			for( q[i++]=t;i<rep->length;++i ){
				q[i]=towlower( rep->data[i] );
			}
			return String( p );
		}
		return *this;
	}

	String ToUpper()const{
		for( int i=0;i<rep->length;++i ){
			Char t=towupper( rep->data[i] );
			if( t==rep->data[i] ) continue;
			Rep *p=Rep::alloc( rep->length );
			Char *q=p->data;
			t_memcpy( q,rep->data,i );
			for( q[i++]=t;i<rep->length;++i ){
				q[i]=towupper( rep->data[i] );
			}
			return String( p );
		}
		return *this;
	}
	
	bool Contains( String sub )const{
		return Find( sub )!=-1;
	}

	bool StartsWith( String sub )const{
		return sub.rep->length<=rep->length && !t_memcmp( rep->data,sub.rep->data,sub.rep->length );
	}

	bool EndsWith( String sub )const{
		return sub.rep->length<=rep->length && !t_memcmp( rep->data+rep->length-sub.rep->length,sub.rep->data,sub.rep->length );
	}
	
	String Slice( int from,int term )const{
		int len=rep->length;
		if( from<0 ){
			from+=len;
			if( from<0 ) from=0;
		}else if( from>len ){
			from=len;
		}
		if( term<0 ){
			term+=len;
		}else if( term>len ){
			term=len;
		}
		if( term<from ) return String();
		if( from==0 && term==len ) return *this;
		return String( rep->data+from,term-from );
	}

	String Slice( int from )const{
		return Slice( from,rep->length );
	}
	
	Array<int> ToChars()const{
		Array<int> chars( rep->length );
		for( int i=0;i<rep->length;++i ) chars[i]=rep->data[i];
		return chars;
	}
	
	int ToInt()const{
		char buf[64];
		return atoi( ToCString<char>( buf,sizeof(buf) ) );
	}
	
	Float ToFloat()const{
		char buf[256];
		return atof( ToCString<char>( buf,sizeof(buf) ) );
	}

	template<class C> class CString{
		struct Rep{
			int refs;
			C data[1];
		};
		Rep *_rep;
		static Rep _nul;
	public:
		template<class T> CString( const T *data,int length ){
			_rep=(Rep*)malloc( length*sizeof(C)+sizeof(Rep) );
			_rep->refs=1;
			_rep->data[length]=0;
			for( int i=0;i<length;++i ){
				_rep->data[i]=(C)data[i];
			}
		}
		CString():_rep( new Rep ){
			_rep->refs=1;
		}
		CString( const CString &c ):_rep(c._rep){
			++_rep->refs;
		}
		~CString(){
			if( !--_rep->refs ) free( _rep );
		}
		CString &operator=( const CString &c ){
			++c._rep->refs;
			if( !--_rep->refs ) free( _rep );
			_rep=c._rep;
			return *this;
		}
		operator const C*()const{ 
			return _rep->data;
		}
	};
	
	template<class C> CString<C> ToCString()const{
		return CString<C>( rep->data,rep->length );
	}

	template<class C> C *ToCString( C *p,int length )const{
		if( --length>rep->length ) length=rep->length;
		for( int i=0;i<length;++i ) p[i]=rep->data[i];
		p[length]=0;
		return p;
	}
	
#if __OBJC__	
	NSString *ToNSString()const{
		return [NSString stringWithCharacters:ToCString<unichar>() length:rep->length];
	}
#endif

#if __cplusplus_winrt
	Platform::String ^ToWinRTString()const{
		return ref new Platform::String( rep->data,rep->length );
	}
#endif
	CString<char> ToUtf8()const{
		std::vector<unsigned char> buf;
		Save( buf );
		return CString<char>( &buf[0],buf.size() );
	}

	bool Save( FILE *fp )const{
		std::vector<unsigned char> buf;
		Save( buf );
		return buf.size() ? fwrite( &buf[0],1,buf.size(),fp )==buf.size() : true;
	}
	
	void Save( std::vector<unsigned char> &buf )const{
	
		Char *p=rep->data;
		Char *e=p+rep->length;
		
		while( p<e ){
			Char c=*p++;
			if( c<0x80 ){
				buf.push_back( c );
			}else if( c<0x800 ){
				buf.push_back( 0xc0 | (c>>6) );
				buf.push_back( 0x80 | (c & 0x3f) );
			}else{
				buf.push_back( 0xe0 | (c>>12) );
				buf.push_back( 0x80 | ((c>>6) & 0x3f) );
				buf.push_back( 0x80 | (c & 0x3f) );
			}
		}
	}
	
	static String FromChars( Array<int> chars ){
		int n=chars.Length();
		Rep *p=Rep::alloc( n );
		for( int i=0;i<n;++i ){
			p->data[i]=chars[i];
		}
		return String( p );
	}

	static String Load( FILE *fp ){
		unsigned char tmp[4096];
		std::vector<unsigned char> buf;
		for(;;){
			int n=fread( tmp,1,4096,fp );
			if( n>0 ) buf.insert( buf.end(),tmp,tmp+n );
			if( n!=4096 ) break;
		}
		return buf.size() ? String::Load( &buf[0],buf.size() ) : String();
	}
	
	static String Load( unsigned char *p,int n ){
	
		_str_load_err=0;
		
		unsigned char *e=p+n;
		std::vector<Char> chars;
		
		int t0=n>0 ? p[0] : -1;
		int t1=n>1 ? p[1] : -1;

		if( t0==0xfe && t1==0xff ){
			p+=2;
			while( p<e-1 ){
				int c=*p++;
				chars.push_back( (c<<8)|*p++ );
			}
		}else if( t0==0xff && t1==0xfe ){
			p+=2;
			while( p<e-1 ){
				int c=*p++;
				chars.push_back( (*p++<<8)|c );
			}
		}else{
			int t2=n>2 ? p[2] : -1;
			if( t0==0xef && t1==0xbb && t2==0xbf ) p+=3;
			unsigned char *q=p;
			bool fail=false;
			while( p<e ){
				unsigned int c=*p++;
				if( c & 0x80 ){
					if( (c & 0xe0)==0xc0 ){
						if( p>=e || (p[0] & 0xc0)!=0x80 ){
							fail=true;
							break;
						}
						c=((c & 0x1f)<<6) | (p[0] & 0x3f);
						p+=1;
					}else if( (c & 0xf0)==0xe0 ){
						if( p+1>=e || (p[0] & 0xc0)!=0x80 || (p[1] & 0xc0)!=0x80 ){
							fail=true;
							break;
						}
						c=((c & 0x0f)<<12) | ((p[0] & 0x3f)<<6) | (p[1] & 0x3f);
						p+=2;
					}else{
						fail=true;
						break;
					}
				}
				chars.push_back( c );
			}
			if( fail ){
				_str_load_err="Invalid UTF-8";
				return String( q,n );
			}
		}
		return chars.size() ? String( &chars[0],chars.size() ) : String();
	}

private:
	
	struct Rep{
		int refs;
		int length;
		Char data[0];
		
		Rep():refs(1),length(0){
		}
		
		Rep( int length ):refs(1),length(length){
		}
		
		void retain(){
//			atomic_add( &refs,1 );
			++refs;
		}
		
		void release(){
//			if( atomic_sub( &refs,1 )>1 || this==&nullRep ) return;
			if( --refs || this==&nullRep ) return;
			free( this );
		}

		static Rep *alloc( int length ){
			if( !length ) return &nullRep;
			void *p=malloc( sizeof(Rep)+length*sizeof(Char) );
			return new(p) Rep( length );
		}
	};
	Rep *rep;
	
	static Rep nullRep;
	
	String( Rep *rep ):rep(rep){
	}
};

String::Rep String::nullRep;

String *t_create( int n,String *p ){
	for( int i=0;i<n;++i ) new( &p[i] ) String();
	return p+n;
}

String *t_create( int n,String *p,const String *q ){
	for( int i=0;i<n;++i ) new( &p[i] ) String( q[i] );
	return p+n;
}

void t_destroy( int n,String *p ){
	for( int i=0;i<n;++i ) p[i].~String();
}

// ***** Object *****

String dbg_stacktrace();

class Object : public gc_object{
public:
	virtual bool Equals( Object *obj ){
		return this==obj;
	}
	
	virtual int Compare( Object *obj ){
		return (char*)this-(char*)obj;
	}
	
	virtual String debug(){
		return "+Object\n";
	}
};

class ThrowableObject : public Object{
#ifndef NDEBUG
public:
	String stackTrace;
	ThrowableObject():stackTrace( dbg_stacktrace() ){}
#endif
};

struct gc_interface{
	virtual ~gc_interface(){}
};

//***** Debugger *****

//#define Error bbError
//#define Print bbPrint

int bbPrint( String t );

#define dbg_stream stderr

#if _MSC_VER
#define dbg_typeof decltype
#else
#define dbg_typeof __typeof__
#endif 

struct dbg_func;
struct dbg_var_type;

static int dbg_suspend;
static int dbg_stepmode;

const char *dbg_info;
String dbg_exstack;

static void *dbg_var_buf[65536*3];
static void **dbg_var_ptr=dbg_var_buf;

static dbg_func *dbg_func_buf[1024];
static dbg_func **dbg_func_ptr=dbg_func_buf;

String dbg_type( bool *p ){
	return "Bool";
}

String dbg_type( int *p ){
	return "Int";
}

String dbg_type( Float *p ){
	return "Float";
}

String dbg_type( String *p ){
	return "String";
}

template<class T> String dbg_type( T **p ){
	return "Object";
}

template<class T> String dbg_type( Array<T> *p ){
	return dbg_type( &(*p)[0] )+"[]";
}

String dbg_value( bool *p ){
	return *p ? "True" : "False";
}

String dbg_value( int *p ){
	return String( *p );
}

String dbg_value( Float *p ){
	return String( *p );
}

String dbg_value( String *p ){
	String t=*p;
	if( t.Length()>100 ) t=t.Slice( 0,100 )+"...";
	t=t.Replace( "\"","~q" );
	t=t.Replace( "\t","~t" );
	t=t.Replace( "\n","~n" );
	t=t.Replace( "\r","~r" );
	return String("\"")+t+"\"";
}

template<class T> String dbg_value( T **t ){
	Object *p=dynamic_cast<Object*>( *t );
	char buf[64];
	sprintf_s( buf,"%p",p );
	return String("@") + (buf[0]=='0' && buf[1]=='x' ? buf+2 : buf );
}

template<class T> String dbg_value( Array<T> *p ){
	String t="[";
	int n=(*p).Length();
	if( n>100 ) n=100;
	for( int i=0;i<n;++i ){
		if( i ) t+=",";
		t+=dbg_value( &(*p)[i] );
	}
	return t+"]";
}

String dbg_ptr_value( void *p ){
	char buf[64];
	sprintf_s( buf,"%p",p );
	return (buf[0]=='0' && buf[1]=='x' ? buf+2 : buf );
}

template<class T> String dbg_decl( const char *id,T *ptr ){
	return String( id )+":"+dbg_type(ptr)+"="+dbg_value(ptr)+"\n";
}

struct dbg_var_type{
	virtual String type( void *p )=0;
	virtual String value( void *p )=0;
};

template<class T> struct dbg_var_type_t : public dbg_var_type{

	String type( void *p ){
		return dbg_type( (T*)p );
	}
	
	String value( void *p ){
		return dbg_value( (T*)p );
	}
	
	static dbg_var_type_t<T> info;
};
template<class T> dbg_var_type_t<T> dbg_var_type_t<T>::info;

struct dbg_blk{
	void **var_ptr;
	
	dbg_blk():var_ptr(dbg_var_ptr){
		if( dbg_stepmode=='l' ) --dbg_suspend;
	}
	
	~dbg_blk(){
		if( dbg_stepmode=='l' ) ++dbg_suspend;
		dbg_var_ptr=var_ptr;
	}
};

struct dbg_func : public dbg_blk{
	const char *id;
	const char *info;

	dbg_func( const char *p ):id(p),info(dbg_info){
		*dbg_func_ptr++=this;
		if( dbg_stepmode=='s' ) --dbg_suspend;
	}
	
	~dbg_func(){
		if( dbg_stepmode=='s' ) ++dbg_suspend;
		--dbg_func_ptr;
		dbg_info=info;
	}
};

int dbg_print( String t ){
	static char *buf;
	static int len;
	int n=t.Length();
	if( n+100>len ){
		len=n+100;
		free( buf );
		buf=(char*)malloc( len );
	}
	buf[n]='\n';
	for( int i=0;i<n;++i ) buf[i]=t[i];
	fwrite( buf,n+1,1,dbg_stream );
	fflush( dbg_stream );
	return 0;
}

void dbg_callstack(){

	void **var_ptr=dbg_var_buf;
	dbg_func **func_ptr=dbg_func_buf;
	
	while( var_ptr!=dbg_var_ptr ){
		while( func_ptr!=dbg_func_ptr && var_ptr==(*func_ptr)->var_ptr ){
			const char *id=(*func_ptr++)->id;
			const char *info=func_ptr!=dbg_func_ptr ? (*func_ptr)->info : dbg_info;
			fprintf( dbg_stream,"+%s;%s\n",id,info );
		}
		void *vp=*var_ptr++;
		const char *nm=(const char*)*var_ptr++;
		dbg_var_type *ty=(dbg_var_type*)*var_ptr++;
		dbg_print( String(nm)+":"+ty->type(vp)+"="+ty->value(vp) );
	}
	while( func_ptr!=dbg_func_ptr ){
		const char *id=(*func_ptr++)->id;
		const char *info=func_ptr!=dbg_func_ptr ? (*func_ptr)->info : dbg_info;
		fprintf( dbg_stream,"+%s;%s\n",id,info );
	}
}

String dbg_stacktrace(){
	if( !dbg_info || !dbg_info[0] ) return "";
	String str=String( dbg_info )+"\n";
	dbg_func **func_ptr=dbg_func_ptr;
	if( func_ptr==dbg_func_buf ) return str;
	while( --func_ptr!=dbg_func_buf ){
		str+=String( (*func_ptr)->info )+"\n";
	}
	return str;
}

void dbg_throw( const char *err ){
	dbg_exstack=dbg_stacktrace();
	throw err;
}

void dbg_stop(){

#if TARGET_OS_IPHONE
	dbg_throw( "STOP" );
#endif

	fprintf( dbg_stream,"{{~~%s~~}}\n",dbg_info );
	dbg_callstack();
	dbg_print( "" );
	
	for(;;){

		char buf[256];
		char *e=fgets( buf,256,stdin );
		if( !e ) exit( -1 );
		
		e=strchr( buf,'\n' );
		if( !e ) exit( -1 );
		
		*e=0;
		
		Object *p;
		
		switch( buf[0] ){
		case '?':
			break;
		case 'r':	//run
			dbg_suspend=0;		
			dbg_stepmode=0;
			return;
		case 's':	//step
			dbg_suspend=1;
			dbg_stepmode='s';
			return;
		case 'e':	//enter func
			dbg_suspend=1;
			dbg_stepmode='e';
			return;
		case 'l':	//leave block
			dbg_suspend=0;
			dbg_stepmode='l';
			return;
		case '@':	//dump object
			p=0;
			sscanf_s( buf+1,"%p",&p );
			if( p ){
				dbg_print( p->debug() );
			}else{
				dbg_print( "" );
			}
			break;
		case 'q':	//quit!
			exit( 0 );
			break;			
		default:
			printf( "????? %s ?????",buf );fflush( stdout );
			exit( -1 );
		}
	}
}

void dbg_error( const char *err ){

#if TARGET_OS_IPHONE
	dbg_throw( err );
#endif

	for(;;){
		bbPrint( String("Monkey Runtime Error : ")+err );
		bbPrint( dbg_stacktrace() );
		dbg_stop();
	}
}

#define DBG_INFO(X) dbg_info=(X);if( dbg_suspend>0 ) dbg_stop();

#define DBG_ENTER(P) dbg_func _dbg_func(P);

#define DBG_BLOCK() dbg_blk _dbg_blk;

#define DBG_GLOBAL( ID,NAME )	//TODO!

#define DBG_LOCAL( ID,NAME )\
*dbg_var_ptr++=&ID;\
*dbg_var_ptr++=(void*)NAME;\
*dbg_var_ptr++=&dbg_var_type_t<dbg_typeof(ID)>::info;

//**** main ****

int argc;
const char **argv;

Float D2R=0.017453292519943295f;
Float R2D=57.29577951308232f;

int bbPrint( String t ){

	static std::vector<unsigned char> buf;
	buf.clear();
	t.Save( buf );
	buf.push_back( '\n' );
	buf.push_back( 0 );
	
#if __cplusplus_winrt	//winrt?

#if CFG_WINRT_PRINT_ENABLED
	OutputDebugStringA( (const char*)&buf[0] );
#endif

#elif _WIN32			//windows?

	fputs( (const char*)&buf[0],stdout );
	fflush( stdout );

#elif __APPLE__			//macos/ios?

	fputs( (const char*)&buf[0],stdout );
	fflush( stdout );
	
#elif __linux			//linux?

#if CFG_ANDROID_NDK_PRINT_ENABLED
	LOGI( (const char*)&buf[0] );
#else
	fputs( (const char*)&buf[0],stdout );
	fflush( stdout );
#endif

#endif

	return 0;
}

class BBExitApp{
};

int bbError( String err ){
	if( !err.Length() ){
#if __cplusplus_winrt
		throw BBExitApp();
#else
		exit( 0 );
#endif
	}
	dbg_error( err.ToCString<char>() );
	return 0;
}

int bbDebugLog( String t ){
	bbPrint( t );
	return 0;
}

int bbDebugStop(){
	dbg_stop();
	return 0;
}

int bbInit();
int bbMain();

#if _MSC_VER

static void _cdecl seTranslator( unsigned int ex,EXCEPTION_POINTERS *p ){

	switch( ex ){
	case EXCEPTION_ACCESS_VIOLATION:dbg_error( "Memory access violation" );
	case EXCEPTION_ILLEGAL_INSTRUCTION:dbg_error( "Illegal instruction" );
	case EXCEPTION_INT_DIVIDE_BY_ZERO:dbg_error( "Integer divide by zero" );
	case EXCEPTION_STACK_OVERFLOW:dbg_error( "Stack overflow" );
	}
	dbg_error( "Unknown exception" );
}

#else

void sighandler( int sig  ){
	switch( sig ){
	case SIGSEGV:dbg_error( "Memory access violation" );
	case SIGILL:dbg_error( "Illegal instruction" );
	case SIGFPE:dbg_error( "Floating point exception" );
#if !_WIN32
	case SIGBUS:dbg_error( "Bus error" );
#endif	
	}
	dbg_error( "Unknown signal" );
}

#endif

//entry point call by target main()...
//
int bb_std_main( int argc,const char **argv ){

	::argc=argc;
	::argv=argv;
	
#if _MSC_VER

	_set_se_translator( seTranslator );

#else
	
	signal( SIGSEGV,sighandler );
	signal( SIGILL,sighandler );
	signal( SIGFPE,sighandler );
#if !_WIN32
	signal( SIGBUS,sighandler );
#endif

#endif

	if( !setlocale( LC_CTYPE,"en_US.UTF-8" ) ){
		setlocale( LC_CTYPE,"" );
	}

	gc_init1();

	bbInit();
	
	gc_init2();

	bbMain();

	return 0;
}


//***** game.h *****

struct BBGameEvent{
	enum{
		None=0,
		KeyDown=1,KeyUp=2,KeyChar=3,
		MouseDown=4,MouseUp=5,MouseMove=6,
		TouchDown=7,TouchUp=8,TouchMove=9,
		MotionAccel=10
	};
};

class BBGameDelegate : public Object{
public:
	virtual void StartGame(){}
	virtual void SuspendGame(){}
	virtual void ResumeGame(){}
	virtual void UpdateGame(){}
	virtual void RenderGame(){}
	virtual void KeyEvent( int event,int data ){}
	virtual void MouseEvent( int event,int data,Float x,Float y ){}
	virtual void TouchEvent( int event,int data,Float x,Float y ){}
	virtual void MotionEvent( int event,int data,Float x,Float y,Float z ){}
	virtual void DiscardGraphics(){}
};

struct BBDisplayMode : public Object{
	int width;
	int height;
	int depth;
	int hertz;
	int flags;
	BBDisplayMode( int width=0,int height=0,int depth=0,int hertz=0,int flags=0 ):width(width),height(height),depth(depth),hertz(hertz),flags(flags){}
};

class BBGame{
public:
	BBGame();
	virtual ~BBGame(){}
	
	// ***** Extern *****
	static BBGame *Game(){ return _game; }
	
	virtual void SetDelegate( BBGameDelegate *delegate );
	virtual BBGameDelegate *Delegate(){ return _delegate; }
	
	virtual void SetKeyboardEnabled( bool enabled );
	virtual bool KeyboardEnabled();
	
	virtual void SetUpdateRate( int updateRate );
	virtual int UpdateRate();
	
	virtual bool Started(){ return _started; }
	virtual bool Suspended(){ return _suspended; }
	
	virtual int Millisecs();
	virtual void GetDate( Array<int> date );
	virtual int SaveState( String state );
	virtual String LoadState();
	virtual String LoadString( String path );
	virtual bool PollJoystick( int port,Array<Float> joyx,Array<Float> joyy,Array<Float> joyz,Array<bool> buttons );
	virtual void OpenUrl( String url );
	virtual void SetMouseVisible( bool visible );
	
	virtual int GetDeviceWidth(){ return 0; }
	virtual int GetDeviceHeight(){ return 0; }
	virtual void SetDeviceWindow( int width,int height,int flags ){}
	virtual Array<BBDisplayMode*> GetDisplayModes(){ return Array<BBDisplayMode*>(); }
	virtual BBDisplayMode *GetDesktopMode(){ return 0; }
	virtual void SetSwapInterval( int interval ){}

	// ***** Native *****
	virtual String PathToFilePath( String path );
	virtual FILE *OpenFile( String path,String mode );
	virtual unsigned char *LoadData( String path,int *plength );
	virtual unsigned char *LoadImageData( String path,int *width,int *height,int *depth ){ return 0; }
	virtual unsigned char *LoadAudioData( String path,int *length,int *channels,int *format,int *hertz ){ return 0; }
	
	//***** Internal *****
	virtual void Die( ThrowableObject *ex );
	virtual void gc_collect();
	virtual void StartGame();
	virtual void SuspendGame();
	virtual void ResumeGame();
	virtual void UpdateGame();
	virtual void RenderGame();
	virtual void KeyEvent( int ev,int data );
	virtual void MouseEvent( int ev,int data,float x,float y );
	virtual void TouchEvent( int ev,int data,float x,float y );
	virtual void MotionEvent( int ev,int data,float x,float y,float z );
	virtual void DiscardGraphics();
	
protected:

	static BBGame *_game;

	BBGameDelegate *_delegate;
	bool _keyboardEnabled;
	int _updateRate;
	bool _started;
	bool _suspended;
};

//***** game.cpp *****

BBGame *BBGame::_game;

BBGame::BBGame():
_delegate( 0 ),
_keyboardEnabled( false ),
_updateRate( 0 ),
_started( false ),
_suspended( false ){
	_game=this;
}

void BBGame::SetDelegate( BBGameDelegate *delegate ){
	_delegate=delegate;
}

void BBGame::SetKeyboardEnabled( bool enabled ){
	_keyboardEnabled=enabled;
}

bool BBGame::KeyboardEnabled(){
	return _keyboardEnabled;
}

void BBGame::SetUpdateRate( int updateRate ){
	_updateRate=updateRate;
}

int BBGame::UpdateRate(){
	return _updateRate;
}

int BBGame::Millisecs(){
	return 0;
}

void BBGame::GetDate( Array<int> date ){
	int n=date.Length();
	if( n>0 ){
		time_t t=time( 0 );
		
#if _MSC_VER
		struct tm tii;
		struct tm *ti=&tii;
		localtime_s( ti,&t );
#else
		struct tm *ti=localtime( &t );
#endif

		date[0]=ti->tm_year+1900;
		if( n>1 ){ 
			date[1]=ti->tm_mon+1;
			if( n>2 ){
				date[2]=ti->tm_mday;
				if( n>3 ){
					date[3]=ti->tm_hour;
					if( n>4 ){
						date[4]=ti->tm_min;
						if( n>5 ){
							date[5]=ti->tm_sec;
							if( n>6 ){
								date[6]=0;
							}
						}
					}
				}
			}
		}
	}
}

int BBGame::SaveState( String state ){
	if( FILE *f=OpenFile( "./.monkeystate","wb" ) ){
		bool ok=state.Save( f );
		fclose( f );
		return ok ? 0 : -2;
	}
	return -1;
}

String BBGame::LoadState(){
	if( FILE *f=OpenFile( "./.monkeystate","rb" ) ){
		String str=String::Load( f );
		fclose( f );
		return str;
	}
	return "";
}

String BBGame::LoadString( String path ){
	if( FILE *fp=OpenFile( path,"rb" ) ){
		String str=String::Load( fp );
		fclose( fp );
		return str;
	}
	return "";
}

bool BBGame::PollJoystick( int port,Array<Float> joyx,Array<Float> joyy,Array<Float> joyz,Array<bool> buttons ){
	return false;
}

void BBGame::OpenUrl( String url ){
}

void BBGame::SetMouseVisible( bool visible ){
}

//***** C++ Game *****

String BBGame::PathToFilePath( String path ){
	return path;
}

FILE *BBGame::OpenFile( String path,String mode ){
	path=PathToFilePath( path );
	if( path=="" ) return 0;
	
#if __cplusplus_winrt
	path=path.Replace( "/","\\" );
	FILE *f;
	if( _wfopen_s( &f,path.ToCString<wchar_t>(),mode.ToCString<wchar_t>() ) ) return 0;
	return f;
#elif _WIN32
	return _wfopen( path.ToCString<wchar_t>(),mode.ToCString<wchar_t>() );
#else
	return fopen( path.ToCString<char>(),mode.ToCString<char>() );
#endif
}

unsigned char *BBGame::LoadData( String path,int *plength ){

	FILE *f=OpenFile( path,"rb" );
	if( !f ) return 0;

	const int BUF_SZ=4096;
	std::vector<void*> tmps;
	int length=0;
	
	for(;;){
		void *p=malloc( BUF_SZ );
		int n=fread( p,1,BUF_SZ,f );
		tmps.push_back( p );
		length+=n;
		if( n!=BUF_SZ ) break;
	}
	fclose( f );
	
	unsigned char *data=(unsigned char*)malloc( length );
	unsigned char *p=data;
	
	int sz=length;
	for( int i=0;i<tmps.size();++i ){
		int n=sz>BUF_SZ ? BUF_SZ : sz;
		memcpy( p,tmps[i],n );
		free( tmps[i] );
		sz-=n;
		p+=n;
	}
	
	*plength=length;
	
	gc_ext_malloced( length );
	
	return data;
}

//***** INTERNAL *****

void BBGame::Die( ThrowableObject *ex ){
	bbPrint( "Monkey Runtime Error : Uncaught Monkey Exception" );
#ifndef NDEBUG
	bbPrint( ex->stackTrace );
#endif
	exit( -1 );
}

void BBGame::gc_collect(){
	gc_mark( _delegate );
	::gc_collect();
}

void BBGame::StartGame(){

	if( _started ) return;
	_started=true;
	
	try{
		_delegate->StartGame();
	}catch( ThrowableObject *ex ){
		Die( ex );
	}
	gc_collect();
}

void BBGame::SuspendGame(){

	if( !_started || _suspended ) return;
	_suspended=true;
	
	try{
		_delegate->SuspendGame();
	}catch( ThrowableObject *ex ){
		Die( ex );
	}
	gc_collect();
}

void BBGame::ResumeGame(){

	if( !_started || !_suspended ) return;
	_suspended=false;
	
	try{
		_delegate->ResumeGame();
	}catch( ThrowableObject *ex ){
		Die( ex );
	}
	gc_collect();
}

void BBGame::UpdateGame(){

	if( !_started || _suspended ) return;
	
	try{
		_delegate->UpdateGame();
	}catch( ThrowableObject *ex ){
		Die( ex );
	}
	gc_collect();
}

void BBGame::RenderGame(){

	if( !_started ) return;
	
	try{
		_delegate->RenderGame();
	}catch( ThrowableObject *ex ){
		Die( ex );
	}
	gc_collect();
}

void BBGame::KeyEvent( int ev,int data ){

	if( !_started ) return;
	
	try{
		_delegate->KeyEvent( ev,data );
	}catch( ThrowableObject *ex ){
		Die( ex );
	}
	gc_collect();
}

void BBGame::MouseEvent( int ev,int data,float x,float y ){

	if( !_started ) return;
	
	try{
		_delegate->MouseEvent( ev,data,x,y );
	}catch( ThrowableObject *ex ){
		Die( ex );
	}
	gc_collect();
}

void BBGame::TouchEvent( int ev,int data,float x,float y ){

	if( !_started ) return;
	
	try{
		_delegate->TouchEvent( ev,data,x,y );
	}catch( ThrowableObject *ex ){
		Die( ex );
	}
	gc_collect();
}

void BBGame::MotionEvent( int ev,int data,float x,float y,float z ){

	if( !_started ) return;
	
	try{
		_delegate->MotionEvent( ev,data,x,y,z );
	}catch( ThrowableObject *ex ){
		Die( ex );
	}
	gc_collect();
}

void BBGame::DiscardGraphics(){

	if( !_started ) return;
	
	try{
		_delegate->DiscardGraphics();
	}catch( ThrowableObject *ex ){
		Die( ex );
	}
	gc_collect();
}


//***** wavloader.h *****
//
unsigned char *LoadWAV( FILE *f,int *length,int *channels,int *format,int *hertz );

//***** wavloader.cpp *****
//
static const char *readTag( FILE *f ){
	static char buf[8];
	if( fread( buf,4,1,f )!=1 ) return "";
	buf[4]=0;
	return buf;
}

static int readInt( FILE *f ){
	unsigned char buf[4];
	if( fread( buf,4,1,f )!=1 ) return -1;
	return (buf[3]<<24) | (buf[2]<<16) | (buf[1]<<8) | buf[0];
}

static int readShort( FILE *f ){
	unsigned char buf[2];
	if( fread( buf,2,1,f )!=1 ) return -1;
	return (buf[1]<<8) | buf[0];
}

static void skipBytes( int n,FILE *f ){
	char *p=(char*)malloc( n );
	fread( p,n,1,f );
	free(p);
}

unsigned char *LoadWAV( FILE *f,int *plength,int *pchannels,int *pformat,int *phertz ){
	if( !strcmp( readTag( f ),"RIFF" ) ){
		int len=readInt( f )-8;len=len;
		if( !strcmp( readTag( f ),"WAVE" ) ){
			if( !strcmp( readTag( f ),"fmt " ) ){
				int len2=readInt( f );
				int comp=readShort( f );
				if( comp==1 ){
					int chans=readShort( f );
					int hertz=readInt( f );
					int bytespersec=readInt( f );bytespersec=bytespersec;
					int pad=readShort( f );pad=pad;
					int bits=readShort( f );
					int format=bits/8;
					if( len2>16 ) skipBytes( len2-16,f );
					for(;;){
						const char *p=readTag( f );
						if( feof( f ) ) break;
						int size=readInt( f );
						if( strcmp( p,"data" ) ){
							skipBytes( size,f );
							continue;
						}
						unsigned char *data=(unsigned char*)malloc( size );
						if( fread( data,size,1,f )==1 ){
							*plength=size/(chans*format);
							*pchannels=chans;
							*pformat=format;
							*phertz=hertz;
							return data;
						}
						free( data );
					}
				}
			}
		}
	}
	return 0;
}



//***** oggloader.h *****
unsigned char *LoadOGG( FILE *f,int *length,int *channels,int *format,int *hertz );

//***** oggloader.cpp *****
unsigned char *LoadOGG( FILE *f,int *length,int *channels,int *format,int *hertz ){

	int error;
	stb_vorbis *v=stb_vorbis_open_file( f,0,&error,0 );
	if( !v ) return 0;
	
	stb_vorbis_info info=stb_vorbis_get_info( v );
	
	int limit=info.channels*4096;
	int offset=0,data_len=0,total=limit;

	short *data=(short*)malloc( total*sizeof(short) );
	
	for(;;){
		int n=stb_vorbis_get_frame_short_interleaved( v,info.channels,data+offset,total-offset );
		if( !n ) break;
	
		data_len+=n;
		offset+=n*info.channels;
		
		if( offset+limit>total ){
			total*=2;
			data=(short*)realloc( data,total*sizeof(short) );
		}
	}
	
	*length=data_len;
	*channels=info.channels;
	*format=2;
	*hertz=info.sample_rate;
	
	stb_vorbis_close(v);

	return (unsigned char*)data;
}



//***** glfwgame.h *****

class BBGlfwGame : public BBGame{
public:
	BBGlfwGame();
	
	static BBGlfwGame *GlfwGame(){ return _glfwGame; }
	
	virtual void SetUpdateRate( int hertz );
	virtual int Millisecs();
	virtual bool PollJoystick( int port,Array<Float> joyx,Array<Float> joyy,Array<Float> joyz,Array<bool> buttons );
	virtual void OpenUrl( String url );
	virtual void SetMouseVisible( bool visible );
		
	virtual int GetDeviceWidth(){ return _width; }
	virtual int GetDeviceHeight(){ return _height; }
	virtual void SetDeviceWindow( int width,int height,int flags );
	virtual void SetSwapInterval( int interval );
	virtual Array<BBDisplayMode*> GetDisplayModes();
	virtual BBDisplayMode *GetDesktopMode();

	virtual String PathToFilePath( String path );
	virtual unsigned char *LoadImageData( String path,int *width,int *height,int *format );
	virtual unsigned char *LoadAudioData( String path,int *length,int *channels,int *format,int *hertz );
	
	void Run();
	
	GLFWwindow *GetGLFWwindow(){ return _window; }
		
private:
	static BBGlfwGame *_glfwGame;
	
	GLFWvidmode _desktopMode;
	
	GLFWwindow *_window;
	int _width;
	int _height;
	int _swapInterval;
	bool _focus;

	double _updatePeriod;
	double _nextUpdate;
	
	double GetTime();
	void Sleep( double time );
	void UpdateEvents();

//	void SetGlfwWindow( int width,int height,int red,int green,int blue,int alpha,int depth,int stencil,bool fullscreen );
		
	static int TransKey( int key );
	static int KeyToChar( int key );
	
	static void OnKey( GLFWwindow *window,int key,int scancode,int action,int mods );
	static void OnChar( GLFWwindow *window,unsigned int chr );
	static void OnMouseButton( GLFWwindow *window,int button,int action,int mods );
	static void OnCursorPos( GLFWwindow *window,double x,double y );
	static void OnWindowClose( GLFWwindow *window );
	static void OnWindowSize( GLFWwindow *window,int width,int height );
};

//***** glfwgame.cpp *****

#include <errno.h>

#define _QUOTE(X) #X
#define _STRINGIZE( X ) _QUOTE(X)

enum{
	VKEY_BACKSPACE=8,VKEY_TAB,
	VKEY_ENTER=13,
	VKEY_SHIFT=16,
	VKEY_CONTROL=17,
	VKEY_ESCAPE=27,
	VKEY_SPACE=32,
	VKEY_PAGE_UP=33,VKEY_PAGE_DOWN,VKEY_END,VKEY_HOME,
	VKEY_LEFT=37,VKEY_UP,VKEY_RIGHT,VKEY_DOWN,
	VKEY_INSERT=45,VKEY_DELETE,
	VKEY_0=48,VKEY_1,VKEY_2,VKEY_3,VKEY_4,VKEY_5,VKEY_6,VKEY_7,VKEY_8,VKEY_9,
	VKEY_A=65,VKEY_B,VKEY_C,VKEY_D,VKEY_E,VKEY_F,VKEY_G,VKEY_H,VKEY_I,VKEY_J,
	VKEY_K,VKEY_L,VKEY_M,VKEY_N,VKEY_O,VKEY_P,VKEY_Q,VKEY_R,VKEY_S,VKEY_T,
	VKEY_U,VKEY_V,VKEY_W,VKEY_X,VKEY_Y,VKEY_Z,
	
	VKEY_LSYS=91,VKEY_RSYS,
	
	VKEY_NUM0=96,VKEY_NUM1,VKEY_NUM2,VKEY_NUM3,VKEY_NUM4,
	VKEY_NUM5,VKEY_NUM6,VKEY_NUM7,VKEY_NUM8,VKEY_NUM9,
	VKEY_NUMMULTIPLY=106,VKEY_NUMADD,VKEY_NUMSLASH,
	VKEY_NUMSUBTRACT,VKEY_NUMDECIMAL,VKEY_NUMDIVIDE,

	VKEY_F1=112,VKEY_F2,VKEY_F3,VKEY_F4,VKEY_F5,VKEY_F6,
	VKEY_F7,VKEY_F8,VKEY_F9,VKEY_F10,VKEY_F11,VKEY_F12,

	VKEY_LEFT_SHIFT=160,VKEY_RIGHT_SHIFT,
	VKEY_LEFT_CONTROL=162,VKEY_RIGHT_CONTROL,
	VKEY_LEFT_ALT=164,VKEY_RIGHT_ALT,

	VKEY_TILDE=192,VKEY_MINUS=189,VKEY_EQUALS=187,
	VKEY_OPENBRACKET=219,VKEY_BACKSLASH=220,VKEY_CLOSEBRACKET=221,
	VKEY_SEMICOLON=186,VKEY_QUOTES=222,
	VKEY_COMMA=188,VKEY_PERIOD=190,VKEY_SLASH=191
};

void Init_GL_Exts();

int glfwGraphicsSeq=0;

BBGlfwGame *BBGlfwGame::_glfwGame;

BBGlfwGame::BBGlfwGame():_window(0),_width(0),_height(0),_swapInterval(1),_focus(true),_updatePeriod(0),_nextUpdate(0){
	_glfwGame=this;

	memset( &_desktopMode,0,sizeof(_desktopMode) );	
	const GLFWvidmode *vmode=glfwGetVideoMode( glfwGetPrimaryMonitor() );
	if( vmode ) _desktopMode=*vmode;
}

void BBGlfwGame::SetUpdateRate( int updateRate ){
	BBGame::SetUpdateRate( updateRate );
	if( _updateRate ) _updatePeriod=1.0/_updateRate;
	_nextUpdate=0;
}

int BBGlfwGame::Millisecs(){
	return int( GetTime()*1000.0 );
}

bool BBGlfwGame::PollJoystick( int port,Array<Float> joyx,Array<Float> joyy,Array<Float> joyz,Array<bool> buttons ){

	static int pjoys[4];
	if( !port ){
		int i=0;
		for( int joy=0;joy<16 && i<4;++joy ){
			if( glfwJoystickPresent( joy ) ) pjoys[i++]=joy;
		}
		while( i<4 ) pjoys[i++]=-1;
	}
	port=pjoys[port];
	if( port==-1 ) return false;
	
	//read axes
	int n_axes=0;
	const float *axes=glfwGetJoystickAxes( port,&n_axes );
	
	//read buttons
	int n_buts=0;
	const unsigned char *buts=glfwGetJoystickButtons( port,&n_buts );

	//Ugh...
	
	const int *dev_axes;
	const int *dev_buttons;
	
#if _WIN32
	
	//xbox 360 controller
	const int xbox360_axes[]={0,0x41,2,4,0x43,0x42,999};
	const int xbox360_buttons[]={0,1,2,3,4,5,6,7,13,10,11,12,8,9,999};
	
	//logitech dual action
	const int logitech_axes[]={0,1,0x86,2,0x43,0x87,999};
	const int logitech_buttons[]={1,2,0,3,4,5,8,9,15,12,13,14,10,11,999};
	
	if( n_axes==5 && n_buts==14 ){
		dev_axes=xbox360_axes;
		dev_buttons=xbox360_buttons;
	}else{
		dev_axes=logitech_axes;
		dev_buttons=logitech_buttons;
	}
	
#else

	//xbox 360 controller
	const int xbox360_axes[]={0,0x41,0x14,2,0x43,0x15,999};
	const int xbox360_buttons[]={11,12,13,14,8,9,5,4,2,0,3,1,6,7,10,999};

	//ps3 controller
	const int ps3_axes[]={0,0x41,0x88,2,0x43,0x89,999};
	const int ps3_buttons[]={14,13,15,12,10,11,0,3,7,4,5,6,1,2,16,999};

	//logitech dual action
	const int logitech_axes[]={0,0x41,0x86,2,0x43,0x87,999};
	const int logitech_buttons[]={1,2,0,3,4,5,8,9,15,12,13,14,10,11,999};

	if( n_axes==6 && n_buts==15 ){
		dev_axes=xbox360_axes;
		dev_buttons=xbox360_buttons;
	}else if( n_axes==4 && n_buts==19 ){
		dev_axes=ps3_axes;
		dev_buttons=ps3_buttons;
	}else{
		dev_axes=logitech_axes;
		dev_buttons=logitech_buttons;
	}

#endif

	const int *p=dev_axes;
	
	float joys[6]={0,0,0,0,0,0};
	
	for( int i=0;i<6 && p[i]!=999;++i ){
		int j=p[i]&0xf,k=p[i]&~0xf;
		if( k==0x10 ){
			joys[i]=(axes[j]+1)/2;
		}else if( k==0x20 ){
			joys[i]=(1-axes[j])/2;
		}else if( k==0x40 ){
			joys[i]=-axes[j];
		}else if( k==0x80 ){
			joys[i]=(buts[j]==GLFW_PRESS);
		}else{
			joys[i]=axes[j];
		}
	}
	
	joyx[0]=joys[0];joyy[0]=joys[1];joyz[0]=joys[2];
	joyx[1]=joys[3];joyy[1]=joys[4];joyz[1]=joys[5];
	
	p=dev_buttons;
	
	for( int i=0;i<32;++i ) buttons[i]=false;
	
	for( int i=0;i<32 && p[i]!=999;++i ){
		int j=p[i];
		if( j<0 ) j+=n_buts;
		buttons[i]=(buts[j]==GLFW_PRESS);
	}

	return true;
}

void BBGlfwGame::OpenUrl( String url ){
#if _WIN32
	ShellExecute( HWND_DESKTOP,"open",url.ToCString<char>(),0,0,SW_SHOWNORMAL );
#elif __APPLE__
	if( CFURLRef cfurl=CFURLCreateWithBytes( 0,url.ToCString<UInt8>(),url.Length(),kCFStringEncodingASCII,0 ) ){
		LSOpenCFURLRef( cfurl,0 );
		CFRelease( cfurl );
	}
#elif __linux
	system( ( String( "xdg-open \"" )+url+"\"" ).ToCString<char>() );
#endif
}

void BBGlfwGame::SetMouseVisible( bool visible ){
	if( visible ){
		glfwSetInputMode( _window,GLFW_CURSOR,GLFW_CURSOR_NORMAL );
	}else{
		glfwSetInputMode( _window,GLFW_CURSOR,GLFW_CURSOR_HIDDEN );
	}
}

String BBGlfwGame::PathToFilePath( String path ){
	if( !path.StartsWith( "monkey:" ) ){
		return path;
	}else if( path.StartsWith( "monkey://data/" ) ){
		return String("./data/")+path.Slice( 14 );
	}else if( path.StartsWith( "monkey://internal/" ) ){
		return String("./internal/")+path.Slice( 18 );
	}else if( path.StartsWith( "monkey://external/" ) ){
		return String("./external/")+path.Slice( 18 );
	}
	return "";
}

unsigned char *BBGlfwGame::LoadImageData( String path,int *width,int *height,int *depth ){

	FILE *f=OpenFile( path,"rb" );
	if( !f ) return 0;
	
	unsigned char *data=stbi_load_from_file( f,width,height,depth,0 );
	fclose( f );
	
	if( data ) gc_ext_malloced( (*width)*(*height)*(*depth) );
	
	return data;
}

/*
extern "C" unsigned char *load_image_png( FILE *f,int *width,int *height,int *format );
extern "C" unsigned char *load_image_jpg( FILE *f,int *width,int *height,int *format );

unsigned char *BBGlfwGame::LoadImageData( String path,int *width,int *height,int *depth ){

	FILE *f=OpenFile( path,"rb" );
	if( !f ) return 0;

	unsigned char *data=0;
	
	if( path.ToLower().EndsWith( ".png" ) ){
		data=load_image_png( f,width,height,depth );
	}else if( path.ToLower().EndsWith( ".jpg" ) || path.ToLower().EndsWith( ".jpeg" ) ){
		data=load_image_jpg( f,width,height,depth );
	}else{
		//meh?
	}

	fclose( f );
	
	if( data ) gc_ext_malloced( (*width)*(*height)*(*depth) );
	
	return data;
}
*/

unsigned char *BBGlfwGame::LoadAudioData( String path,int *length,int *channels,int *format,int *hertz ){

	FILE *f=OpenFile( path,"rb" );
	if( !f ) return 0;
	
	unsigned char *data=0;
	
	if( path.ToLower().EndsWith( ".wav" ) ){
		data=LoadWAV( f,length,channels,format,hertz );
	}else if( path.ToLower().EndsWith( ".ogg" ) ){
		data=LoadOGG( f,length,channels,format,hertz );
	}
	fclose( f );
	
	if( data ) gc_ext_malloced( (*length)*(*channels)*(*format) );
	
	return data;
}

//glfw key to monkey key!
int BBGlfwGame::TransKey( int key ){

	if( key>='0' && key<='9' ) return key;
	if( key>='A' && key<='Z' ) return key;

	switch( key ){
	case ' ':return VKEY_SPACE;
	case ';':return VKEY_SEMICOLON;
	case '=':return VKEY_EQUALS;
	case ',':return VKEY_COMMA;
	case '-':return VKEY_MINUS;
	case '.':return VKEY_PERIOD;
	case '/':return VKEY_SLASH;
	case '~':return VKEY_TILDE;
	case '[':return VKEY_OPENBRACKET;
	case ']':return VKEY_CLOSEBRACKET;
	case '\"':return VKEY_QUOTES;
	case '\\':return VKEY_BACKSLASH;
	
	case '`':return VKEY_TILDE;
	case '\'':return VKEY_QUOTES;

	case GLFW_KEY_LEFT_SHIFT:
	case GLFW_KEY_RIGHT_SHIFT:return VKEY_SHIFT;
	case GLFW_KEY_LEFT_CONTROL:
	case GLFW_KEY_RIGHT_CONTROL:return VKEY_CONTROL;
	
//	case GLFW_KEY_LEFT_SHIFT:return VKEY_LEFT_SHIFT;
//	case GLFW_KEY_RIGHT_SHIFT:return VKEY_RIGHT_SHIFT;
//	case GLFW_KEY_LCTRL:return VKEY_LCONTROL;
//	case GLFW_KEY_RCTRL:return VKEY_RCONTROL;
	
	case GLFW_KEY_BACKSPACE:return VKEY_BACKSPACE;
	case GLFW_KEY_TAB:return VKEY_TAB;
	case GLFW_KEY_ENTER:return VKEY_ENTER;
	case GLFW_KEY_ESCAPE:return VKEY_ESCAPE;
	case GLFW_KEY_INSERT:return VKEY_INSERT;
	case GLFW_KEY_DELETE:return VKEY_DELETE;
	case GLFW_KEY_PAGE_UP:return VKEY_PAGE_UP;
	case GLFW_KEY_PAGE_DOWN:return VKEY_PAGE_DOWN;
	case GLFW_KEY_HOME:return VKEY_HOME;
	case GLFW_KEY_END:return VKEY_END;
	case GLFW_KEY_UP:return VKEY_UP;
	case GLFW_KEY_DOWN:return VKEY_DOWN;
	case GLFW_KEY_LEFT:return VKEY_LEFT;
	case GLFW_KEY_RIGHT:return VKEY_RIGHT;
	
	case GLFW_KEY_KP_0:return VKEY_NUM0;
	case GLFW_KEY_KP_1:return VKEY_NUM1;
	case GLFW_KEY_KP_2:return VKEY_NUM2;
	case GLFW_KEY_KP_3:return VKEY_NUM3;
	case GLFW_KEY_KP_4:return VKEY_NUM4;
	case GLFW_KEY_KP_5:return VKEY_NUM5;
	case GLFW_KEY_KP_6:return VKEY_NUM6;
	case GLFW_KEY_KP_7:return VKEY_NUM7;
	case GLFW_KEY_KP_8:return VKEY_NUM8;
	case GLFW_KEY_KP_9:return VKEY_NUM9;
	case GLFW_KEY_KP_DIVIDE:return VKEY_NUMDIVIDE;
	case GLFW_KEY_KP_MULTIPLY:return VKEY_NUMMULTIPLY;
	case GLFW_KEY_KP_SUBTRACT:return VKEY_NUMSUBTRACT;
	case GLFW_KEY_KP_ADD:return VKEY_NUMADD;
	case GLFW_KEY_KP_DECIMAL:return VKEY_NUMDECIMAL;
	
	case GLFW_KEY_F1:return VKEY_F1;
	case GLFW_KEY_F2:return VKEY_F2;
	case GLFW_KEY_F3:return VKEY_F3;
	case GLFW_KEY_F4:return VKEY_F4;
	case GLFW_KEY_F5:return VKEY_F5;
	case GLFW_KEY_F6:return VKEY_F6;
	case GLFW_KEY_F7:return VKEY_F7;
	case GLFW_KEY_F8:return VKEY_F8;
	case GLFW_KEY_F9:return VKEY_F9;
	case GLFW_KEY_F10:return VKEY_F10;
	case GLFW_KEY_F11:return VKEY_F11;
	case GLFW_KEY_F12:return VKEY_F12;
	}
	return 0;
}

//monkey key to special monkey char
int BBGlfwGame::KeyToChar( int key ){
	switch( key ){
	case VKEY_BACKSPACE:
	case VKEY_TAB:
	case VKEY_ENTER:
	case VKEY_ESCAPE:
		return key;
	case VKEY_PAGE_UP:
	case VKEY_PAGE_DOWN:
	case VKEY_END:
	case VKEY_HOME:
	case VKEY_LEFT:
	case VKEY_UP:
	case VKEY_RIGHT:
	case VKEY_DOWN:
	case VKEY_INSERT:
		return key | 0x10000;
	case VKEY_DELETE:
		return 127;
	}
	return 0;
}

void BBGlfwGame::OnKey( GLFWwindow *window,int key,int scancode,int action,int mods ){

	key=TransKey( key );
	if( !key ) return;
	
	switch( action ){
	case GLFW_PRESS:
	case GLFW_REPEAT:
		_glfwGame->KeyEvent( BBGameEvent::KeyDown,key );
		if( int chr=KeyToChar( key ) ) _glfwGame->KeyEvent( BBGameEvent::KeyChar,chr );
		break;
	case GLFW_RELEASE:
		_glfwGame->KeyEvent( BBGameEvent::KeyUp,key );
		break;
	}
}

void BBGlfwGame::OnChar( GLFWwindow *window,unsigned int chr ){

	_glfwGame->KeyEvent( BBGameEvent::KeyChar,chr );
}

void BBGlfwGame::OnMouseButton( GLFWwindow *window,int button,int action,int mods ){
	switch( button ){
	case GLFW_MOUSE_BUTTON_LEFT:button=0;break;
	case GLFW_MOUSE_BUTTON_RIGHT:button=1;break;
	case GLFW_MOUSE_BUTTON_MIDDLE:button=2;break;
	default:return;
	}
	double x=0,y=0;
	glfwGetCursorPos( window,&x,&y );
	switch( action ){
	case GLFW_PRESS:
		_glfwGame->MouseEvent( BBGameEvent::MouseDown,button,x,y );
		break;
	case GLFW_RELEASE:
		_glfwGame->MouseEvent( BBGameEvent::MouseUp,button,x,y );
		break;
	}
}

void BBGlfwGame::OnCursorPos( GLFWwindow *window,double x,double y ){
	_glfwGame->MouseEvent( BBGameEvent::MouseMove,-1,x,y );
}

void BBGlfwGame::OnWindowClose( GLFWwindow *window ){
	_glfwGame->KeyEvent( BBGameEvent::KeyDown,0x1b0 );
	_glfwGame->KeyEvent( BBGameEvent::KeyUp,0x1b0 );
}

void BBGlfwGame::OnWindowSize( GLFWwindow *window,int width,int height ){

	_glfwGame->_width=width;
	_glfwGame->_height=height;
	
#if CFG_GLFW_WINDOW_RENDER_WHILE_RESIZING && !__linux
	_glfwGame->RenderGame();
	glfwSwapBuffers( _glfwGame->_window );
	_glfwGame->_nextUpdate=0;
#endif
}

void BBGlfwGame::SetDeviceWindow( int width,int height,int flags ){

	_focus=false;

	if( _window ){
		for( int i=0;i<=GLFW_KEY_LAST;++i ){
			int key=TransKey( i );
			if( key && glfwGetKey( _window,i )==GLFW_PRESS ) KeyEvent( BBGameEvent::KeyUp,key );
		}
		glfwDestroyWindow( _window );
		_window=0;
	}

	bool fullscreen=(flags & 1);
	bool resizable=(flags & 2);
	bool decorated=(flags & 4);
	bool floating=(flags & 8);
	bool depthbuffer=(flags & 16);
	bool doublebuffer=!(flags & 32);
	bool secondmonitor=(flags & 64);

	glfwWindowHint( GLFW_RED_BITS,8 );
	glfwWindowHint( GLFW_GREEN_BITS,8 );
	glfwWindowHint( GLFW_BLUE_BITS,8 );
	glfwWindowHint( GLFW_ALPHA_BITS,0 );
	glfwWindowHint( GLFW_DEPTH_BITS,depthbuffer ? 32 : 0 );
	glfwWindowHint( GLFW_STENCIL_BITS,0 );
	glfwWindowHint( GLFW_RESIZABLE,resizable );
	glfwWindowHint( GLFW_DECORATED,decorated );
	glfwWindowHint( GLFW_FLOATING,floating );
	glfwWindowHint( GLFW_VISIBLE,fullscreen );
	glfwWindowHint( GLFW_DOUBLEBUFFER,doublebuffer );
	glfwWindowHint( GLFW_SAMPLES,CFG_GLFW_WINDOW_SAMPLES );
	glfwWindowHint( GLFW_REFRESH_RATE,60 );
	
	GLFWmonitor *monitor=0;
	if( fullscreen ){
		int monitorid=secondmonitor ? 1 : 0;
		int count=0;
		GLFWmonitor **monitors=glfwGetMonitors( &count );
		if( monitorid>=count ) monitorid=count-1;
		monitor=monitors[monitorid];
	}
	
	_window=glfwCreateWindow( width,height,_STRINGIZE(CFG_GLFW_WINDOW_TITLE),monitor,0 );
	if( !_window ){
		bbPrint( "glfwCreateWindow FAILED!" );
		abort();
	}
	
	_width=width;
	_height=height;
	
	++glfwGraphicsSeq;

	if( !fullscreen ){	
		glfwSetWindowPos( _window,(_desktopMode.width-width)/2,(_desktopMode.height-height)/2 );
		glfwShowWindow( _window );
	}
	
	glfwMakeContextCurrent( _window );
	
	if( _swapInterval>=0 ) glfwSwapInterval( _swapInterval );

#if CFG_OPENGL_INIT_EXTENSIONS
	Init_GL_Exts();
#endif

	glfwSetKeyCallback( _window,OnKey );
	glfwSetCharCallback( _window,OnChar );
	glfwSetMouseButtonCallback( _window,OnMouseButton );
	glfwSetCursorPosCallback( _window,OnCursorPos );
	glfwSetWindowCloseCallback(	_window,OnWindowClose );
	glfwSetWindowSizeCallback(_window,OnWindowSize );
}

void BBGlfwGame::SetSwapInterval( int interval ){

	_swapInterval=interval;
	
	if( _swapInterval>=0 && _window ) glfwSwapInterval( _swapInterval );
}

Array<BBDisplayMode*> BBGlfwGame::GetDisplayModes(){
	int count=0;
	const GLFWvidmode *vmodes=glfwGetVideoModes( glfwGetPrimaryMonitor(),&count );
	Array<BBDisplayMode*> modes( count );
	int n=0;
	for( int i=0;i<count;++i ){
		const GLFWvidmode *vmode=&vmodes[i];
		if( vmode->refreshRate && vmode->refreshRate!=60 ) continue;
		modes[n++]=new BBDisplayMode( vmode->width,vmode->height );
	}
	return modes.Slice(0,n);
}

BBDisplayMode *BBGlfwGame::GetDesktopMode(){ 
	return new BBDisplayMode( _desktopMode.width,_desktopMode.height ); 
}

double BBGlfwGame::GetTime(){
	return glfwGetTime();
}

void BBGlfwGame::Sleep( double time ){
#if _WIN32
	WaitForSingleObject( GetCurrentThread(),(DWORD)( time*1000.0 ) );
#else
	timespec ts,rem;
	ts.tv_sec=floor(time);
	ts.tv_nsec=(time-floor(time))*1000000000.0;
	while( nanosleep( &ts,&rem )==EINTR ){
		ts=rem;
	}
#endif
}

void BBGlfwGame::UpdateEvents(){

	if( _suspended ){
		glfwWaitEvents();
	}else{
		glfwPollEvents();
	}
	if( glfwGetWindowAttrib( _window,GLFW_FOCUSED ) ){
		_focus=true;
		if( _suspended ){
			ResumeGame();
			_nextUpdate=0;
		}
	}else if( glfwGetWindowAttrib( _window,GLFW_ICONIFIED ) || CFG_MOJO_AUTO_SUSPEND_ENABLED ){
		if( _focus && !_suspended ){
			SuspendGame();
			_nextUpdate=0;
		}
	}
}

void BBGlfwGame::Run(){

#if	CFG_GLFW_WINDOW_WIDTH && CFG_GLFW_WINDOW_HEIGHT

	int flags=0;
#if CFG_GLFW_WINDOW_FULLSCREEN
	flags|=1;
#endif
#if CFG_GLFW_WINDOW_RESIZABLE
	flags|=2;
#endif
#if CFG_GLFW_WINDOW_DECORATED
	flags|=4;
#endif
#if CFG_GLFW_WINDOW_FLOATING
	flags|=8;
#endif
#if CFG_OPENGL_DEPTH_BUFFER_ENABLED
	flags|=16;
#endif

	SetDeviceWindow( CFG_GLFW_WINDOW_WIDTH,CFG_GLFW_WINDOW_HEIGHT,flags );

#endif

	StartGame();
	
	while( !glfwWindowShouldClose( _window ) ){
	
		RenderGame();
		
		glfwSwapBuffers( _window );
		
		//Wait for next update
		if( _nextUpdate ){
			double delay=_nextUpdate-GetTime();
			if( delay>0 ) Sleep( delay );
		}
		
		//Update user events
		UpdateEvents();

		//App suspended?		
		if( _suspended ){
			continue;
		}

		//'Go nuts' mode!
		if( !_updateRate ){
			UpdateGame();
			continue;
		}
		
		//Reset update timer?
		if( !_nextUpdate ){
			_nextUpdate=GetTime();
		}
		
		//Catch up updates...
		int i=0;
		for( ;i<4;++i ){
		
			UpdateGame();
			if( !_nextUpdate ) break;
			
			_nextUpdate+=_updatePeriod;
			
			if( _nextUpdate>GetTime() ) break;
		}
		
		if( i==4 ) _nextUpdate=0;
	}
}



//***** monkeygame.h *****

class BBMonkeyGame : public BBGlfwGame{
public:
	static void Main( int args,const char *argv[] );
};

//***** monkeygame.cpp *****

#define _QUOTE(X) #X
#define _STRINGIZE(X) _QUOTE(X)

static void onGlfwError( int err,const char *msg ){
	printf( "GLFW Error: err=%i, msg=%s\n",err,msg );
	fflush( stdout );
}

void BBMonkeyGame::Main( int argc,const char *argv[] ){

	glfwSetErrorCallback( onGlfwError );
	
	if( !glfwInit() ){
		puts( "glfwInit failed" );
		exit( -1 );
	}

	BBMonkeyGame *game=new BBMonkeyGame();
	
	try{
	
		bb_std_main( argc,argv );
		
	}catch( ThrowableObject *ex ){
	
		glfwTerminate();
		
		game->Die( ex );
		
		return;
	}

	if( game->Delegate() ) game->Run();
	
	glfwTerminate();
}


// GLFW mojo runtime.
//
// Copyright 2011 Mark Sibly, all rights reserved.
// No warranty implied; use at your own risk.

//***** gxtkGraphics.h *****

class gxtkSurface;

class gxtkGraphics : public Object{
public:

	enum{
		MAX_VERTS=1024,
		MAX_QUADS=(MAX_VERTS/4)
	};

	int width;
	int height;

	int colorARGB;
	float r,g,b,alpha;
	float ix,iy,jx,jy,tx,ty;
	bool tformed;

	float vertices[MAX_VERTS*5];
	unsigned short quadIndices[MAX_QUADS*6];

	int primType;
	int vertCount;
	gxtkSurface *primSurf;
	
	gxtkGraphics();
	
	void Flush();
	float *Begin( int type,int count,gxtkSurface *surf );
	
	//***** GXTK API *****
	virtual int Width();
	virtual int Height();
	
	virtual int  BeginRender();
	virtual void EndRender();
	virtual void DiscardGraphics();

	virtual gxtkSurface *LoadSurface( String path );
	virtual gxtkSurface *CreateSurface( int width,int height );
	virtual bool LoadSurface__UNSAFE__( gxtkSurface *surface,String path );
	
	virtual int Cls( float r,float g,float b );
	virtual int SetAlpha( float alpha );
	virtual int SetColor( float r,float g,float b );
	virtual int SetBlend( int blend );
	virtual int SetScissor( int x,int y,int w,int h );
	virtual int SetMatrix( float ix,float iy,float jx,float jy,float tx,float ty );
	
	virtual int DrawPoint( float x,float y );
	virtual int DrawRect( float x,float y,float w,float h );
	virtual int DrawLine( float x1,float y1,float x2,float y2 );
	virtual int DrawOval( float x1,float y1,float x2,float y2 );
	virtual int DrawPoly( Array<Float> verts );
	virtual int DrawPoly2( Array<Float> verts,gxtkSurface *surface,int srcx,int srcy );
	virtual int DrawSurface( gxtkSurface *surface,float x,float y );
	virtual int DrawSurface2( gxtkSurface *surface,float x,float y,int srcx,int srcy,int srcw,int srch );
	
	virtual int ReadPixels( Array<int> pixels,int x,int y,int width,int height,int offset,int pitch );
	virtual int WritePixels2( gxtkSurface *surface,Array<int> pixels,int x,int y,int width,int height,int offset,int pitch );
};

class gxtkSurface : public Object{
public:
	unsigned char *data;
	int width;
	int height;
	int depth;
	int format;
	int seq;
	
	GLuint texture;
	float uscale;
	float vscale;
	
	gxtkSurface();
	
	void SetData( unsigned char *data,int width,int height,int depth );
	void SetSubData( int x,int y,int w,int h,unsigned *src,int pitch );
	void Bind();
	
	~gxtkSurface();
	
	//***** GXTK API *****
	virtual int Discard();
	virtual int Width();
	virtual int Height();
	virtual int Loaded();
	virtual void OnUnsafeLoadComplete();
};

//***** gxtkGraphics.cpp *****

#ifndef GL_BGRA
#define GL_BGRA  0x80e1
#endif

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812f
#endif

#ifndef GL_GENERATE_MIPMAP
#define GL_GENERATE_MIPMAP 0x8191
#endif

static int Pow2Size( int n ){
	int i=1;
	while( i<n ) i+=i;
	return i;
}

gxtkGraphics::gxtkGraphics(){

	width=height=0;
	vertCount=0;
	
#ifdef _glfw3_h_
	GLFWwindow *window=BBGlfwGame::GlfwGame()->GetGLFWwindow();
	if( window ) glfwGetWindowSize( BBGlfwGame::GlfwGame()->GetGLFWwindow(),&width,&height );
#else
	glfwGetWindowSize( &width,&height );
#endif
	
	if( CFG_OPENGL_GLES20_ENABLED ) return;
	
	for( int i=0;i<MAX_QUADS;++i ){
		quadIndices[i*6  ]=(short)(i*4);
		quadIndices[i*6+1]=(short)(i*4+1);
		quadIndices[i*6+2]=(short)(i*4+2);
		quadIndices[i*6+3]=(short)(i*4);
		quadIndices[i*6+4]=(short)(i*4+2);
		quadIndices[i*6+5]=(short)(i*4+3);
	}
}

void gxtkGraphics::Flush(){
	if( !vertCount ) return;

	if( primSurf ){
		glEnable( GL_TEXTURE_2D );
		primSurf->Bind();
	}
		
	switch( primType ){
	case 1:
		glDrawArrays( GL_POINTS,0,vertCount );
		break;
	case 2:
		glDrawArrays( GL_LINES,0,vertCount );
		break;
	case 3:
		glDrawArrays( GL_TRIANGLES,0,vertCount );
		break;
	case 4:
		glDrawElements( GL_TRIANGLES,vertCount/4*6,GL_UNSIGNED_SHORT,quadIndices );
		break;
	default:
		for( int j=0;j<vertCount;j+=primType ){
			glDrawArrays( GL_TRIANGLE_FAN,j,primType );
		}
		break;
	}

	if( primSurf ){
		glDisable( GL_TEXTURE_2D );
	}

	vertCount=0;
}

float *gxtkGraphics::Begin( int type,int count,gxtkSurface *surf ){
	if( primType!=type || primSurf!=surf || vertCount+count>MAX_VERTS ){
		Flush();
		primType=type;
		primSurf=surf;
	}
	float *vp=vertices+vertCount*5;
	vertCount+=count;
	return vp;
}

//***** GXTK API *****

int gxtkGraphics::Width(){
	return width;
}

int gxtkGraphics::Height(){
	return height;
}

int gxtkGraphics::BeginRender(){

	width=height=0;
#ifdef _glfw3_h_
	glfwGetWindowSize( BBGlfwGame::GlfwGame()->GetGLFWwindow(),&width,&height );
#else
	glfwGetWindowSize( &width,&height );
#endif

#if CFG_OPENGL_GLES20_ENABLED
	return 0;
#else

	glViewport( 0,0,width,height );

	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	glOrtho( 0,width,height,0,-1,1 );
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();
	
	glEnableClientState( GL_VERTEX_ARRAY );
	glVertexPointer( 2,GL_FLOAT,20,&vertices[0] );	
	
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	glTexCoordPointer( 2,GL_FLOAT,20,&vertices[2] );
	
	glEnableClientState( GL_COLOR_ARRAY );
	glColorPointer( 4,GL_UNSIGNED_BYTE,20,&vertices[4] );
	
	glEnable( GL_BLEND );
	glBlendFunc( GL_ONE,GL_ONE_MINUS_SRC_ALPHA );
	
	glDisable( GL_TEXTURE_2D );
	
	vertCount=0;
	
	return 1;
	
#endif
}

void gxtkGraphics::EndRender(){
	if( !CFG_OPENGL_GLES20_ENABLED ) Flush();
}

void gxtkGraphics::DiscardGraphics(){
}

int gxtkGraphics::Cls( float r,float g,float b ){
	vertCount=0;

	glClearColor( r/255.0f,g/255.0f,b/255.0f,1 );
	glClear( GL_COLOR_BUFFER_BIT );

	return 0;
}

int gxtkGraphics::SetAlpha( float alpha ){
	this->alpha=alpha;
	
	int a=int(alpha*255);
	
	colorARGB=(a<<24) | (int(b*alpha)<<16) | (int(g*alpha)<<8) | int(r*alpha);
	
	return 0;
}

int gxtkGraphics::SetColor( float r,float g,float b ){
	this->r=r;
	this->g=g;
	this->b=b;

	int a=int(alpha*255);
	
	colorARGB=(a<<24) | (int(b*alpha)<<16) | (int(g*alpha)<<8) | int(r*alpha);
	
	return 0;
}

int gxtkGraphics::SetBlend( int blend ){

	Flush();
	
	switch( blend ){
	case 1:
		glBlendFunc( GL_ONE,GL_ONE );
		break;
	default:
		glBlendFunc( GL_ONE,GL_ONE_MINUS_SRC_ALPHA );
	}

	return 0;
}

int gxtkGraphics::SetScissor( int x,int y,int w,int h ){

	Flush();
	
	if( x!=0 || y!=0 || w!=Width() || h!=Height() ){
		glEnable( GL_SCISSOR_TEST );
		y=Height()-y-h;
		glScissor( x,y,w,h );
	}else{
		glDisable( GL_SCISSOR_TEST );
	}
	return 0;
}

int gxtkGraphics::SetMatrix( float ix,float iy,float jx,float jy,float tx,float ty ){

	tformed=(ix!=1 || iy!=0 || jx!=0 || jy!=1 || tx!=0 || ty!=0);

	this->ix=ix;this->iy=iy;this->jx=jx;this->jy=jy;this->tx=tx;this->ty=ty;

	return 0;
}

int gxtkGraphics::DrawPoint( float x,float y ){

	if( tformed ){
		float px=x;
		x=px * ix + y * jx + tx;
		y=px * iy + y * jy + ty;
	}
	
	float *vp=Begin( 1,1,0 );
	
	vp[0]=x+.5f;vp[1]=y+.5f;(int&)vp[4]=colorARGB;

	return 0;	
}
	
int gxtkGraphics::DrawLine( float x0,float y0,float x1,float y1 ){

	if( tformed ){
		float tx0=x0,tx1=x1;
		x0=tx0 * ix + y0 * jx + tx;y0=tx0 * iy + y0 * jy + ty;
		x1=tx1 * ix + y1 * jx + tx;y1=tx1 * iy + y1 * jy + ty;
	}
	
	float *vp=Begin( 2,2,0 );

	vp[0]=x0+.5f;vp[1]=y0+.5f;(int&)vp[4]=colorARGB;
	vp[5]=x1+.5f;vp[6]=y1+.5f;(int&)vp[9]=colorARGB;
	
	return 0;
}

int gxtkGraphics::DrawRect( float x,float y,float w,float h ){

	float x0=x,x1=x+w,x2=x+w,x3=x;
	float y0=y,y1=y,y2=y+h,y3=y+h;

	if( tformed ){
		float tx0=x0,tx1=x1,tx2=x2,tx3=x3;
		x0=tx0 * ix + y0 * jx + tx;y0=tx0 * iy + y0 * jy + ty;
		x1=tx1 * ix + y1 * jx + tx;y1=tx1 * iy + y1 * jy + ty;
		x2=tx2 * ix + y2 * jx + tx;y2=tx2 * iy + y2 * jy + ty;
		x3=tx3 * ix + y3 * jx + tx;y3=tx3 * iy + y3 * jy + ty;
	}
	
	float *vp=Begin( 4,4,0 );

	vp[0 ]=x0;vp[1 ]=y0;(int&)vp[4 ]=colorARGB;
	vp[5 ]=x1;vp[6 ]=y1;(int&)vp[9 ]=colorARGB;
	vp[10]=x2;vp[11]=y2;(int&)vp[14]=colorARGB;
	vp[15]=x3;vp[16]=y3;(int&)vp[19]=colorARGB;

	return 0;
}

int gxtkGraphics::DrawOval( float x,float y,float w,float h ){
	
	float xr=w/2.0f;
	float yr=h/2.0f;

	int n;
	if( tformed ){
		float dx_x=xr * ix;
		float dx_y=xr * iy;
		float dx=sqrtf( dx_x*dx_x+dx_y*dx_y );
		float dy_x=yr * jx;
		float dy_y=yr * jy;
		float dy=sqrtf( dy_x*dy_x+dy_y*dy_y );
		n=(int)( dx+dy );
	}else{
		n=(int)( fabs( xr )+fabs( yr ) );
	}
	
	if( n<12 ){
		n=12;
	}else if( n>MAX_VERTS ){
		n=MAX_VERTS;
	}else{
		n&=~3;
	}

	float x0=x+xr,y0=y+yr;
	
	float *vp=Begin( n,n,0 );

	for( int i=0;i<n;++i ){
	
		float th=i * 6.28318531f / n;

		float px=x0+cosf( th ) * xr;
		float py=y0-sinf( th ) * yr;
		
		if( tformed ){
			float ppx=px;
			px=ppx * ix + py * jx + tx;
			py=ppx * iy + py * jy + ty;
		}
		
		vp[0]=px;vp[1]=py;(int&)vp[4]=colorARGB;
		vp+=5;
	}
	
	return 0;
}

int gxtkGraphics::DrawPoly( Array<Float> verts ){

	int n=verts.Length()/2;
	if( n<1 || n>MAX_VERTS ) return 0;
	
	float *vp=Begin( n,n,0 );
	
	for( int i=0;i<n;++i ){
		int j=i*2;
		if( tformed ){
			vp[0]=verts[j] * ix + verts[j+1] * jx + tx;
			vp[1]=verts[j] * iy + verts[j+1] * jy + ty;
		}else{
			vp[0]=verts[j];
			vp[1]=verts[j+1];
		}
		(int&)vp[4]=colorARGB;
		vp+=5;
	}

	return 0;
}

int gxtkGraphics::DrawPoly2( Array<Float> verts,gxtkSurface *surface,int srcx,int srcy ){

	int n=verts.Length()/4;
	if( n<1 || n>MAX_VERTS ) return 0;
		
	float *vp=Begin( n,n,surface );
	
	for( int i=0;i<n;++i ){
		int j=i*4;
		if( tformed ){
			vp[0]=verts[j] * ix + verts[j+1] * jx + tx;
			vp[1]=verts[j] * iy + verts[j+1] * jy + ty;
		}else{
			vp[0]=verts[j];
			vp[1]=verts[j+1];
		}
		vp[2]=(srcx+verts[j+2])*surface->uscale;
		vp[3]=(srcy+verts[j+3])*surface->vscale;
		(int&)vp[4]=colorARGB;
		vp+=5;
	}
	
	return 0;
}

int gxtkGraphics::DrawSurface( gxtkSurface *surf,float x,float y ){
	
	float w=surf->Width();
	float h=surf->Height();
	float x0=x,x1=x+w,x2=x+w,x3=x;
	float y0=y,y1=y,y2=y+h,y3=y+h;
	float u0=0,u1=w*surf->uscale;
	float v0=0,v1=h*surf->vscale;

	if( tformed ){
		float tx0=x0,tx1=x1,tx2=x2,tx3=x3;
		x0=tx0 * ix + y0 * jx + tx;y0=tx0 * iy + y0 * jy + ty;
		x1=tx1 * ix + y1 * jx + tx;y1=tx1 * iy + y1 * jy + ty;
		x2=tx2 * ix + y2 * jx + tx;y2=tx2 * iy + y2 * jy + ty;
		x3=tx3 * ix + y3 * jx + tx;y3=tx3 * iy + y3 * jy + ty;
	}
	
	float *vp=Begin( 4,4,surf );
	
	vp[0 ]=x0;vp[1 ]=y0;vp[2 ]=u0;vp[3 ]=v0;(int&)vp[4 ]=colorARGB;
	vp[5 ]=x1;vp[6 ]=y1;vp[7 ]=u1;vp[8 ]=v0;(int&)vp[9 ]=colorARGB;
	vp[10]=x2;vp[11]=y2;vp[12]=u1;vp[13]=v1;(int&)vp[14]=colorARGB;
	vp[15]=x3;vp[16]=y3;vp[17]=u0;vp[18]=v1;(int&)vp[19]=colorARGB;
	
	return 0;
}

int gxtkGraphics::DrawSurface2( gxtkSurface *surf,float x,float y,int srcx,int srcy,int srcw,int srch ){
	
	float w=srcw;
	float h=srch;
	float x0=x,x1=x+w,x2=x+w,x3=x;
	float y0=y,y1=y,y2=y+h,y3=y+h;
	float u0=srcx*surf->uscale,u1=(srcx+srcw)*surf->uscale;
	float v0=srcy*surf->vscale,v1=(srcy+srch)*surf->vscale;

	if( tformed ){
		float tx0=x0,tx1=x1,tx2=x2,tx3=x3;
		x0=tx0 * ix + y0 * jx + tx;y0=tx0 * iy + y0 * jy + ty;
		x1=tx1 * ix + y1 * jx + tx;y1=tx1 * iy + y1 * jy + ty;
		x2=tx2 * ix + y2 * jx + tx;y2=tx2 * iy + y2 * jy + ty;
		x3=tx3 * ix + y3 * jx + tx;y3=tx3 * iy + y3 * jy + ty;
	}
	
	float *vp=Begin( 4,4,surf );
	
	vp[0 ]=x0;vp[1 ]=y0;vp[2 ]=u0;vp[3 ]=v0;(int&)vp[4 ]=colorARGB;
	vp[5 ]=x1;vp[6 ]=y1;vp[7 ]=u1;vp[8 ]=v0;(int&)vp[9 ]=colorARGB;
	vp[10]=x2;vp[11]=y2;vp[12]=u1;vp[13]=v1;(int&)vp[14]=colorARGB;
	vp[15]=x3;vp[16]=y3;vp[17]=u0;vp[18]=v1;(int&)vp[19]=colorARGB;
	
	return 0;
}
	
int gxtkGraphics::ReadPixels( Array<int> pixels,int x,int y,int width,int height,int offset,int pitch ){

	Flush();

	unsigned *p=(unsigned*)malloc(width*height*4);

	glReadPixels( x,this->height-y-height,width,height,GL_BGRA,GL_UNSIGNED_BYTE,p );
	
	for( int py=0;py<height;++py ){
		memcpy( &pixels[offset+py*pitch],&p[(height-py-1)*width],width*4 );
	}
	
	free( p );
	
	return 0;
}

int gxtkGraphics::WritePixels2( gxtkSurface *surface,Array<int> pixels,int x,int y,int width,int height,int offset,int pitch ){

	surface->SetSubData( x,y,width,height,(unsigned*)&pixels[offset],pitch );
	
	return 0;
}

//***** gxtkSurface *****

gxtkSurface::gxtkSurface():data(0),width(0),height(0),depth(0),format(0),seq(-1),texture(0),uscale(0),vscale(0){
}

gxtkSurface::~gxtkSurface(){
	Discard();
}

int gxtkSurface::Discard(){
	if( seq==glfwGraphicsSeq ){
		glDeleteTextures( 1,&texture );
		seq=-1;
	}
	if( data ){
		free( data );
		data=0;
	}
	return 0;
}

int gxtkSurface::Width(){
	return width;
}

int gxtkSurface::Height(){
	return height;
}

int gxtkSurface::Loaded(){
	return 1;
}

//Careful! Can't call any GL here as it may be executing off-thread.
//
void gxtkSurface::SetData( unsigned char *data,int width,int height,int depth ){

	this->data=data;
	this->width=width;
	this->height=height;
	this->depth=depth;
	
	unsigned char *p=data;
	int n=width*height;
	
	switch( depth ){
	case 1:
		format=GL_LUMINANCE;
		break;
	case 2:
		format=GL_LUMINANCE_ALPHA;
		if( data ){
			while( n-- ){	//premultiply alpha
				p[0]=p[0]*p[1]/255;
				p+=2;
			}
		}
		break;
	case 3:
		format=GL_RGB;
		break;
	case 4:
		format=GL_RGBA;
		if( data ){
			while( n-- ){	//premultiply alpha
				p[0]=p[0]*p[3]/255;
				p[1]=p[1]*p[3]/255;
				p[2]=p[2]*p[3]/255;
				p+=4;
			}
		}
		break;
	}
}

void gxtkSurface::SetSubData( int x,int y,int w,int h,unsigned *src,int pitch ){
	if( format!=GL_RGBA ) return;
	
	if( !data ) data=(unsigned char*)malloc( width*height*4 );
	
	unsigned *dst=(unsigned*)data+y*width+x;
	
	for( int py=0;py<h;++py ){
		unsigned *d=dst+py*width;
		unsigned *s=src+py*pitch;
		for( int px=0;px<w;++px ){
			unsigned p=*s++;
			unsigned a=p>>24;
			*d++=(a<<24) | ((p>>0&0xff)*a/255<<16) | ((p>>8&0xff)*a/255<<8) | ((p>>16&0xff)*a/255);
		}
	}
	
	if( seq==glfwGraphicsSeq ){
		glBindTexture( GL_TEXTURE_2D,texture );
		glPixelStorei( GL_UNPACK_ALIGNMENT,1 );
		if( width==pitch ){
			glTexSubImage2D( GL_TEXTURE_2D,0,x,y,w,h,format,GL_UNSIGNED_BYTE,dst );
		}else{
			for( int py=0;py<h;++py ){
				glTexSubImage2D( GL_TEXTURE_2D,0,x,y+py,w,1,format,GL_UNSIGNED_BYTE,dst+py*width );
			}
		}
	}
}

void gxtkSurface::Bind(){

	if( !glfwGraphicsSeq ) return;
	
	if( seq==glfwGraphicsSeq ){
		glBindTexture( GL_TEXTURE_2D,texture );
		return;
	}
	
	seq=glfwGraphicsSeq;
	
	glGenTextures( 1,&texture );
	glBindTexture( GL_TEXTURE_2D,texture );
	
	if( CFG_MOJO_IMAGE_FILTERING_ENABLED ){
		glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR );
	}else{
		glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST );
	}

	glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE );

	int texwidth=width;
	int texheight=height;
	
	glTexImage2D( GL_TEXTURE_2D,0,format,texwidth,texheight,0,format,GL_UNSIGNED_BYTE,0 );
	if( glGetError()!=GL_NO_ERROR ){
		texwidth=Pow2Size( width );
		texheight=Pow2Size( height );
		glTexImage2D( GL_TEXTURE_2D,0,format,texwidth,texheight,0,format,GL_UNSIGNED_BYTE,0 );
	}
	
	uscale=1.0/texwidth;
	vscale=1.0/texheight;
	
	if( data ){
		glPixelStorei( GL_UNPACK_ALIGNMENT,1 );
		glTexSubImage2D( GL_TEXTURE_2D,0,0,0,width,height,format,GL_UNSIGNED_BYTE,data );
	}
}

void gxtkSurface::OnUnsafeLoadComplete(){
	Bind();
}

bool gxtkGraphics::LoadSurface__UNSAFE__( gxtkSurface *surface,String path ){

	int width,height,depth;
	unsigned char *data=BBGlfwGame::GlfwGame()->LoadImageData( path,&width,&height,&depth );
	if( !data ) return false;
	
	surface->SetData( data,width,height,depth );
	return true;
}

gxtkSurface *gxtkGraphics::LoadSurface( String path ){
	gxtkSurface *surf=new gxtkSurface();
	if( !LoadSurface__UNSAFE__( surf,path ) ) return 0;
	surf->Bind();
	return surf;
}

gxtkSurface *gxtkGraphics::CreateSurface( int width,int height ){
	gxtkSurface *surf=new gxtkSurface();
	surf->SetData( 0,width,height,4 );
	surf->Bind();
	return surf;
}

//***** gxtkAudio.h *****

class gxtkSample;

class gxtkChannel{
public:
	ALuint source;
	gxtkSample *sample;
	int flags;
	int state;
	
	int AL_Source();
};

class gxtkAudio : public Object{
public:
	static gxtkAudio *audio;
	
	ALCdevice *alcDevice;
	ALCcontext *alcContext;
	gxtkChannel channels[33];

	gxtkAudio();

	virtual void mark();

	//***** GXTK API *****
	virtual int Suspend();
	virtual int Resume();

	virtual gxtkSample *LoadSample( String path );
	virtual bool LoadSample__UNSAFE__( gxtkSample *sample,String path );
	
	virtual int PlaySample( gxtkSample *sample,int channel,int flags );

	virtual int StopChannel( int channel );
	virtual int PauseChannel( int channel );
	virtual int ResumeChannel( int channel );
	virtual int ChannelState( int channel );
	virtual int SetVolume( int channel,float volume );
	virtual int SetPan( int channel,float pan );
	virtual int SetRate( int channel,float rate );
	
	virtual int PlayMusic( String path,int flags );
	virtual int StopMusic();
	virtual int PauseMusic();
	virtual int ResumeMusic();
	virtual int MusicState();
	virtual int SetMusicVolume( float volume );
};

class gxtkSample : public Object{
public:
	ALuint al_buffer;

	gxtkSample();
	gxtkSample( ALuint buf );
	~gxtkSample();
	
	void SetBuffer( ALuint buf );
	
	//***** GXTK API *****
	virtual int Discard();
};

//***** gxtkAudio.cpp *****

gxtkAudio *gxtkAudio::audio;

static std::vector<ALuint> discarded;

static void FlushDiscarded(){

	if( !discarded.size() ) return;
	
	for( int i=0;i<33;++i ){
		gxtkChannel *chan=&gxtkAudio::audio->channels[i];
		if( chan->state ){
			int state=0;
			alGetSourcei( chan->source,AL_SOURCE_STATE,&state );
			if( state==AL_STOPPED ) alSourcei( chan->source,AL_BUFFER,0 );
		}
	}
	
	std::vector<ALuint> out;
	
	for( int i=0;i<discarded.size();++i ){
		ALuint buf=discarded[i];
		alDeleteBuffers( 1,&buf );
		ALenum err=alGetError();
		if( err==AL_NO_ERROR ){
//			printf( "alDeleteBuffers OK!\n" );fflush( stdout );
		}else{
//			printf( "alDeleteBuffers failed...\n" );fflush( stdout );
			out.push_back( buf );
		}
	}
	discarded=out;
}

int gxtkChannel::AL_Source(){
	if( source ) return source;

	alGetError();
	alGenSources( 1,&source );
	if( alGetError()==AL_NO_ERROR ) return source;
	
	//couldn't create source...steal a free source...?
	//
	source=0;
	for( int i=0;i<32;++i ){
		gxtkChannel *chan=&gxtkAudio::audio->channels[i];
		if( !chan->source || gxtkAudio::audio->ChannelState( i ) ) continue;
//		puts( "Stealing source!" );
		source=chan->source;
		chan->source=0;
		break;
	}
	return source;
}

gxtkAudio::gxtkAudio(){

	audio=this;
	
	alcDevice=alcOpenDevice( 0 );
	if( !alcDevice ){
		alcDevice=alcOpenDevice( "Generic Hardware" );
		if( !alcDevice ) alcDevice=alcOpenDevice( "Generic Software" );
	}

//	bbPrint( "opening openal device" );
	if( alcDevice ){
		if( (alcContext=alcCreateContext( alcDevice,0 )) ){
			if( (alcMakeContextCurrent( alcContext )) ){
				//alc all go!
			}else{
				bbPrint( "OpenAl error: alcMakeContextCurrent failed" );
			}
		}else{
			bbPrint( "OpenAl error: alcCreateContext failed" );
		}
	}else{
		bbPrint( "OpenAl error: alcOpenDevice failed" );
	}

	alDistanceModel( AL_NONE );
	
	memset( channels,0,sizeof(channels) );

	channels[32].AL_Source();
}

void gxtkAudio::mark(){
	for( int i=0;i<33;++i ){
		gxtkChannel *chan=&channels[i];
		if( chan->state!=0 ){
			int state=0;
			alGetSourcei( chan->source,AL_SOURCE_STATE,&state );
			if( state!=AL_STOPPED ) gc_mark( chan->sample );
		}
	}
}

int gxtkAudio::Suspend(){
	for( int i=0;i<33;++i ){
		gxtkChannel *chan=&channels[i];
		if( chan->state==1 ){
			int state=0;
			alGetSourcei( chan->source,AL_SOURCE_STATE,&state );
			if( state==AL_PLAYING ) alSourcePause( chan->source );
		}
	}
	return 0;
}

int gxtkAudio::Resume(){
	for( int i=0;i<33;++i ){
		gxtkChannel *chan=&channels[i];
		if( chan->state==1 ){
			int state=0;
			alGetSourcei( chan->source,AL_SOURCE_STATE,&state );
			if( state==AL_PAUSED ) alSourcePlay( chan->source );
		}
	}
	return 0;
}

bool gxtkAudio::LoadSample__UNSAFE__( gxtkSample *sample,String path ){

	int length=0;
	int channels=0;
	int format=0;
	int hertz=0;
	unsigned char *data=BBGlfwGame::GlfwGame()->LoadAudioData( path,&length,&channels,&format,&hertz );
	if( !data ) return false;
	
	int al_format=0;
	if( format==1 && channels==1 ){
		al_format=AL_FORMAT_MONO8;
	}else if( format==1 && channels==2 ){
		al_format=AL_FORMAT_STEREO8;
	}else if( format==2 && channels==1 ){
		al_format=AL_FORMAT_MONO16;
	}else if( format==2 && channels==2 ){
		al_format=AL_FORMAT_STEREO16;
	}
	
	int size=length*channels*format;
	
	ALuint al_buffer;
	alGenBuffers( 1,&al_buffer );
	alBufferData( al_buffer,al_format,data,size,hertz );
	free( data );
	
	sample->SetBuffer( al_buffer );
	return true;
}

gxtkSample *gxtkAudio::LoadSample( String path ){
	FlushDiscarded();
	gxtkSample *sample=new gxtkSample();
	if( !LoadSample__UNSAFE__( sample,path ) ) return 0;
	return sample;
}

int gxtkAudio::PlaySample( gxtkSample *sample,int channel,int flags ){

	FlushDiscarded();
	
	gxtkChannel *chan=&channels[channel];
	
	if( !chan->AL_Source() ) return -1;
	
	alSourceStop( chan->source );
	alSourcei( chan->source,AL_BUFFER,sample->al_buffer );
	alSourcei( chan->source,AL_LOOPING,flags ? 1 : 0 );
	alSourcePlay( chan->source );
	
	gc_assign( chan->sample,sample );

	chan->flags=flags;
	chan->state=1;

	return 0;
}

int gxtkAudio::StopChannel( int channel ){
	gxtkChannel *chan=&channels[channel];

	if( chan->state!=0 ){
		alSourceStop( chan->source );
		chan->state=0;
	}
	return 0;
}

int gxtkAudio::PauseChannel( int channel ){
	gxtkChannel *chan=&channels[channel];

	if( chan->state==1 ){
		int state=0;
		alGetSourcei( chan->source,AL_SOURCE_STATE,&state );
		if( state==AL_STOPPED ){
			chan->state=0;
		}else{
			alSourcePause( chan->source );
			chan->state=2;
		}
	}
	return 0;
}

int gxtkAudio::ResumeChannel( int channel ){
	gxtkChannel *chan=&channels[channel];

	if( chan->state==2 ){
		alSourcePlay( chan->source );
		chan->state=1;
	}
	return 0;
}

int gxtkAudio::ChannelState( int channel ){
	gxtkChannel *chan=&channels[channel];
	
	if( chan->state==1 ){
		int state=0;
		alGetSourcei( chan->source,AL_SOURCE_STATE,&state );
		if( state==AL_STOPPED ) chan->state=0;
	}
	return chan->state;
}

int gxtkAudio::SetVolume( int channel,float volume ){
	gxtkChannel *chan=&channels[channel];

	alSourcef( chan->AL_Source(),AL_GAIN,volume );
	return 0;
}

int gxtkAudio::SetPan( int channel,float pan ){
	gxtkChannel *chan=&channels[channel];
	
	float x=sinf( pan ),y=0,z=-cosf( pan );
	alSource3f( chan->AL_Source(),AL_POSITION,x,y,z );
	return 0;
}

int gxtkAudio::SetRate( int channel,float rate ){
	gxtkChannel *chan=&channels[channel];

	alSourcef( chan->AL_Source(),AL_PITCH,rate );
	return 0;
}

int gxtkAudio::PlayMusic( String path,int flags ){
	StopMusic();
	
	gxtkSample *music=LoadSample( path );
	if( !music ) return -1;
	
	PlaySample( music,32,flags );
	return 0;
}

int gxtkAudio::StopMusic(){
	StopChannel( 32 );
	return 0;
}

int gxtkAudio::PauseMusic(){
	PauseChannel( 32 );
	return 0;
}

int gxtkAudio::ResumeMusic(){
	ResumeChannel( 32 );
	return 0;
}

int gxtkAudio::MusicState(){
	return ChannelState( 32 );
}

int gxtkAudio::SetMusicVolume( float volume ){
	SetVolume( 32,volume );
	return 0;
}

gxtkSample::gxtkSample():
al_buffer(0){
}

gxtkSample::gxtkSample( ALuint buf ):
al_buffer(buf){
}

gxtkSample::~gxtkSample(){
	Discard();
}

void gxtkSample::SetBuffer( ALuint buf ){
	al_buffer=buf;
}

int gxtkSample::Discard(){
	if( al_buffer ){
		discarded.push_back( al_buffer );
		al_buffer=0;
	}
	return 0;
}


// ***** thread.h *****

#if __cplusplus_winrt

using namespace Windows::System::Threading;

#endif

class BBThread : public Object{
public:
	Object *result;
	
	BBThread();
	
	virtual void Start();
	virtual bool IsRunning();
	
	virtual Object *Result();
	virtual void SetResult( Object *result );
	
	static  String Strdup( const String &str );
	
	virtual void Run__UNSAFE__();
	
	
private:

	enum{
		INIT=0,
		RUNNING=1,
		FINISHED=2
	};

	
	int _state;
	Object *_result;
	
#if __cplusplus_winrt

	friend class Launcher;

	class Launcher{
	
		friend class BBThread;
		BBThread *_thread;
		
		Launcher( BBThread *thread ):_thread(thread){
		}
		
		public:
		
		void operator()( IAsyncAction ^operation ){
			_thread->Run__UNSAFE__();
			_thread->_state=FINISHED;
		} 
	};
	
#elif _WIN32

	static DWORD WINAPI run( void *p );
	
#else

	static void *run( void *p );
	
#endif

};

// ***** thread.cpp *****

BBThread::BBThread():_state( INIT ),_result( 0 ){
}

bool BBThread::IsRunning(){
	return _state==RUNNING;
}

Object *BBThread::Result(){
	return _result;
}

void BBThread::SetResult( Object *result ){
	_result=result;
}

String BBThread::Strdup( const String &str ){
	return str.Copy();
}

void BBThread::Run__UNSAFE__(){
}

#if __cplusplus_winrt

void BBThread::Start(){
	if( _state==RUNNING ) return;
	
	_result=0;
	_state=RUNNING;
	
	Launcher launcher( this );
	
	auto handler=ref new WorkItemHandler( launcher );
	
	ThreadPool::RunAsync( handler );
}

#elif _WIN32

void BBThread::Start(){
	if( _state==RUNNING ) return;
	
	_result=0;
	_state=RUNNING;
	
	DWORD _id;
	HANDLE _handle;

	if( _handle=CreateThread( 0,0,run,this,0,&_id ) ){
		CloseHandle( _handle );
		return;
	}
	
	puts( "CreateThread failed!" );
	exit( -1 );
}

DWORD WINAPI BBThread::run( void *p ){
	BBThread *thread=(BBThread*)p;

	thread->Run__UNSAFE__();
	
	thread->_state=FINISHED;
	return 0;
}

#else

void BBThread::Start(){
	if( _state==RUNNING ) return;
	
	_result=0;
	_state=RUNNING;
	
	pthread_t _handle;
	
	if( !pthread_create( &_handle,0,run,this ) ){
		pthread_detach( _handle );
		return;
	}
	
	puts( "pthread_create failed!" );
	exit( -1 );
}

void *BBThread::run( void *p ){
	BBThread *thread=(BBThread*)p;

	thread->Run__UNSAFE__();

	thread->_state=FINISHED;
	return 0;
}

#endif

#include <Shellapi.h>

class browser
{
	public:

	
	static void launchBrowser(String address, String windowName)
	{
		LPCSTR addressStr = address.ToCString<char>();
		ShellExecute(HWND_DESKTOP, "open", addressStr, NULL, NULL, SW_SHOWNORMAL);
	}

};
class c_App;
class c_Game;
class c_GameDelegate;
class c_Image;
class c_GraphicsContext;
class c_Frame;
class c_InputDevice;
class c_JoyState;
class c_DisplayMode;
class c_Map;
class c_IntMap;
class c_Stack;
class c_Node;
class c_BBGameEvent;
class c_VirtualDisplay;
class c_Font;
class c_BitmapFont;
class c_BitMapChar;
class c_DrawingPoint;
class c_BitMapCharMetrics;
class c_Box2D_World;
class c_b2World;
class c_b2Vec2;
class c_b2DestructionListener;
class c_b2DebugDraw;
class c_b2Body;
class c_b2Contact;
class c_b2Joint;
class c_b2Controller;
class c_b2ContactManager;
class c_b2ContactFilter;
class c_b2ContactListenerInterface;
class c_b2ContactListener;
class c_b2ContactFactory;
class c_b2ContactRegister;
class c_FlashArray;
class c_FlashArray2;
class c_b2Shape;
class c_ContactTypeFactory;
class c_CircleContactTypeFactory;
class c_PolyAndCircleContactTypeFactory;
class c_PolygonContactTypeFactory;
class c_EdgeAndCircleContactTypeFactory;
class c_PolyAndEdgeContactTypeFactory;
class c_IBroadPhase;
class c_b2DynamicTreeBroadPhase;
class c_UpdatePairsCallback;
class c_CMUpdatePairsCallback;
class c_b2BodyDef;
class c_b2Transform;
class c_b2Mat22;
class c_b2Sweep;
class c_b2JointEdge;
class c_b2ControllerEdge;
class c_b2ContactEdge;
class c_b2Fixture;
class c_Entity;
class c_List;
class c_Node2;
class c_HeadNode;
class c_Barrier;
class c_List2;
class c_Node3;
class c_HeadNode2;
class c_GameData;
class c_StageData;
class c_LevelData;
class c_Enumerator;
class c_b2Manifold;
class c_b2ManifoldPoint;
class c_b2Settings;
class c_b2ContactID;
class c_Features;
class c_b2PolygonShape;
class c_b2CircleShape;
class c_b2FixtureDef;
class c_Polygon;
class c_List3;
class c_Node4;
class c_HeadNode3;
class c_Enumerator2;
class c_b2Math;
class c_Constants;
class c_b2FilterData;
class c_b2AABB;
class c_b2MassData;
class c_Enumerator3;
class c_StringObject;
class c_SensorContactListener;
class c_b2TimeStep;
class c_b2DistanceInput;
class c_b2DistanceProxy;
class c_b2SimplexCache;
class c_b2DistanceOutput;
class c_b2Distance;
class c_b2Simplex;
class c_b2SimplexVertex;
class c_b2Island;
class c_b2ContactSolver;
class c_b2ContactConstraint;
class c_b2ContactConstraintPoint;
class c_b2WorldManifold;
class c_b2PositionSolverManifold;
class c_b2ContactImpulse;
class c_b2TOIInput;
class c_b2TimeOfImpact;
class c_b2SeparationFunction;
class c_DoTweet;
class c_eDrawAlign;
class c_eDrawMode;
class c_b2Color;
class c_b2EdgeShape;
class c_b2PulleyJoint;
class c_b2PolyAndCircleContact;
class c_b2DynamicTreeNode;
class c_b2DynamicTree;
class c_FlashArray3;
class c_QueryCallback;
class c_TreeQueryCallback;
class c_DTQueryCallback;
class c_b2DynamicTreePair;
class c_FlashArray4;
class c_b2Collision;
class c_b2CircleContact;
class c_b2PolygonContact;
class c_b2EdgeAndCircleContact;
class c_b2PolyAndEdgeContact;
class c_ClipVertex;
class c_App : public Object{
	public:
	c_App();
	c_App* m_new();
	int p_OnResize();
	virtual int p_OnCreate();
	int p_OnSuspend();
	int p_OnResume();
	virtual int p_OnUpdate();
	int p_OnLoading();
	virtual int p_OnRender();
	int p_OnClose();
	int p_OnBack();
	void mark();
};
class c_Game : public c_App{
	public:
	c_Image* m_img_player;
	c_Image* m_img_mainMenu;
	c_Image* m_img_stageSelect;
	c_Image* m_img_levelComplete;
	c_Image* m_img_level;
	c_Image* m_img_redo;
	c_Image* m_img_menu;
	c_Image* m_img_exit;
	c_Image* m_img_starFull;
	c_Image* m_img_starFullLarge;
	c_Image* m_img_levelH;
	c_Image* m_img_starFullH;
	c_Image* m_img_rightArrowH;
	c_Image* m_img_leftArrowH;
	c_Image* m_img_menuH1;
	c_Image* m_img_optionsH1;
	c_Image* m_img_playH;
	c_Image* m_img_tutorialH;
	c_Image* m_img_optionsH2;
	c_Image* m_img_menuH2;
	c_Image* m_img_menuH3;
	c_Image* m_img_nextH;
	c_Image* m_img_twitterH;
	c_Image* m_img_redoH;
	c_Image* m_img_barrierV;
	c_Image* m_img_barrierH;
	c_Image* m_img_cross1;
	c_Image* m_img_cube1;
	c_Image* m_img_options;
	c_Image* m_img_res1H;
	c_Image* m_img_res2H;
	c_Image* m_img_res3H;
	c_Image* m_img_windowedH;
	c_Image* m_img_fullscreenH;
	c_Image* m_img_creditsH;
	c_Image* m_img_exitGameH;
	c_Image* m_img_returnH;
	c_Image* m_img_stage2;
	c_Image* m_img_stage3;
	c_Image* m_img_completed;
	c_Image* m_img_androidOptions;
	c_Image* m_img_on;
	c_Image* m_img_off;
	c_BitmapFont* m_fnt_stageFont;
	c_BitmapFont* m_fnt_stageSelectFont;
	c_BitmapFont* m_fnt_stageSelectFontH;
	c_BitmapFont* m_fnt_72Font;
	c_BitmapFont* m_fnt_54Font;
	c_BitmapFont* m_fnt_timeFont;
	bool m_debug;
	c_Box2D_World* m_world;
	bool m_tutorial;
	int m_tutStep;
	bool m_options;
	bool m_fullscreen;
	int m_resX;
	int m_resY;
	bool m_credits;
	int m_creditStep;
	int m_level;
	int m_stage;
	bool m_hasWon;
	int m_levelStartTimer;
	Float m_finalTime;
	c_List2* m_barrierList;
	int m_touchDelayStart;
	int m_touchDelayTime;
	bool m_main_menu;
	bool m_isPlaying;
	bool m_levelComplete;
	int m_oldSelector;
	int m_selector;
	Float m_mX;
	Float m_mY;
	bool m_gamepad;
	int m_gameProgress;
	c_GameData* m_gameData;
	bool m_musicPlaying;
	int m_playerFriction;
	c_Entity* m_player;
	Float m_maxAngularVelocity;
	Float m_exitX;
	Float m_exitY;
	String m_fTime;
	int m_tStars;
	int m_tutTime;
	int m_tutWait;
	int m_gameProgressTimer;
	int m_creditStart;
	c_Game();
	c_Game* m_new();
	c_Entity* p_CreateLevel1();
	c_Entity* p_CreateLevel2();
	c_Entity* p_CreateLevel3();
	c_Entity* p_CreateLevel4();
	c_Entity* p_CreateLevel5_2();
	c_Entity* p_CreateLevel5();
	c_Entity* p_CreateLevel6_2();
	c_Entity* p_CreateLevel6_3();
	c_Entity* p_CreateLevel6();
	c_Entity* p_CreateLevel7();
	c_Entity* p_CreateLevel8();
	c_Entity* p_CreateLevel9();
	c_Entity* p_CreateLevel10();
	c_Entity* p_CreateLevel11_2();
	c_Entity* p_CreateLevel11();
	c_Entity* p_CreateLevel13_2();
	c_Entity* p_CreateLevel13_3();
	c_Entity* p_CreateLevel13();
	c_Entity* p_CreateLevel17();
	c_Entity* p_CreateLevel18();
	c_Entity* p_CreateLevel19_2();
	c_Entity* p_CreateLevel19_3();
	c_Entity* p_CreateLevel19();
	c_Entity* p_CreateLevel20();
	c_Entity* p_CreateLevel22_2();
	c_Entity* p_CreateLevel22_3();
	c_Entity* p_CreateLevel22();
	c_Entity* p_CreateLevel23();
	void p_BuildLevel(int);
	int p_OnCreate();
	void p_ResetBarriers(int);
	void p_CreateExit(Float,Float);
	void p_CreateExit2(int);
	void p_ResetPlayer(int);
	void p_LoadLevel2(int);
	void p_LoadLevelBackground(int);
	void p_LoadLevel(int);
	static bool m_sensorColliding;
	void p_SetResolution(int,int,bool);
	int p_OnUpdate();
	void p_DrawLevel(int);
	int p_OnRender();
	void mark();
};
extern c_App* bb_app__app;
class c_GameDelegate : public BBGameDelegate{
	public:
	gxtkGraphics* m__graphics;
	gxtkAudio* m__audio;
	c_InputDevice* m__input;
	c_GameDelegate();
	c_GameDelegate* m_new();
	void StartGame();
	void SuspendGame();
	void ResumeGame();
	void UpdateGame();
	void RenderGame();
	void KeyEvent(int,int);
	void MouseEvent(int,int,Float,Float);
	void TouchEvent(int,int,Float,Float);
	void MotionEvent(int,int,Float,Float,Float);
	void DiscardGraphics();
	void mark();
};
extern c_GameDelegate* bb_app__delegate;
extern BBGame* bb_app__game;
int bbMain();
extern gxtkGraphics* bb_graphics_device;
int bb_graphics_SetGraphicsDevice(gxtkGraphics*);
class c_Image : public Object{
	public:
	gxtkSurface* m_surface;
	int m_width;
	int m_height;
	Array<c_Frame* > m_frames;
	int m_flags;
	Float m_tx;
	Float m_ty;
	c_Image* m_source;
	c_Image();
	static int m_DefaultFlags;
	c_Image* m_new();
	int p_SetHandle(Float,Float);
	int p_ApplyFlags(int);
	c_Image* p_Init(gxtkSurface*,int,int);
	c_Image* p_Init2(gxtkSurface*,int,int,int,int,int,int,c_Image*,int,int,int,int);
	int p_Width();
	int p_Height();
	int p_Discard();
	void mark();
};
class c_GraphicsContext : public Object{
	public:
	c_Image* m_defaultFont;
	c_Image* m_font;
	int m_firstChar;
	int m_matrixSp;
	Float m_ix;
	Float m_iy;
	Float m_jx;
	Float m_jy;
	Float m_tx;
	Float m_ty;
	int m_tformed;
	int m_matDirty;
	Float m_color_r;
	Float m_color_g;
	Float m_color_b;
	Float m_alpha;
	int m_blend;
	Float m_scissor_x;
	Float m_scissor_y;
	Float m_scissor_width;
	Float m_scissor_height;
	Array<Float > m_matrixStack;
	c_GraphicsContext();
	c_GraphicsContext* m_new();
	int p_Validate();
	void mark();
};
extern c_GraphicsContext* bb_graphics_context;
String bb_data_FixDataPath(String);
class c_Frame : public Object{
	public:
	int m_x;
	int m_y;
	c_Frame();
	c_Frame* m_new(int,int);
	c_Frame* m_new2();
	void mark();
};
c_Image* bb_graphics_LoadImage(String,int,int);
c_Image* bb_graphics_LoadImage2(String,int,int,int,int);
int bb_graphics_SetFont(c_Image*,int);
extern gxtkAudio* bb_audio_device;
int bb_audio_SetAudioDevice(gxtkAudio*);
class c_InputDevice : public Object{
	public:
	Array<c_JoyState* > m__joyStates;
	Array<bool > m__keyDown;
	int m__keyHitPut;
	Array<int > m__keyHitQueue;
	Array<int > m__keyHit;
	int m__charGet;
	int m__charPut;
	Array<int > m__charQueue;
	Float m__mouseX;
	Float m__mouseY;
	Array<Float > m__touchX;
	Array<Float > m__touchY;
	Float m__accelX;
	Float m__accelY;
	Float m__accelZ;
	c_InputDevice();
	c_InputDevice* m_new();
	void p_PutKeyHit(int);
	void p_BeginUpdate();
	void p_EndUpdate();
	void p_KeyEvent(int,int);
	void p_MouseEvent(int,int,Float,Float);
	void p_TouchEvent(int,int,Float,Float);
	void p_MotionEvent(int,int,Float,Float,Float);
	Float p_TouchX(int);
	Float p_TouchY(int);
	bool p_KeyDown(int);
	Float p_JoyZ(int,int);
	int p_KeyHit(int);
	void mark();
};
class c_JoyState : public Object{
	public:
	Array<Float > m_joyx;
	Array<Float > m_joyy;
	Array<Float > m_joyz;
	Array<bool > m_buttons;
	c_JoyState();
	c_JoyState* m_new();
	void mark();
};
extern c_InputDevice* bb_input_device;
int bb_input_SetInputDevice(c_InputDevice*);
extern int bb_app__devWidth;
extern int bb_app__devHeight;
void bb_app_ValidateDeviceWindow(bool);
class c_DisplayMode : public Object{
	public:
	int m__width;
	int m__height;
	c_DisplayMode();
	c_DisplayMode* m_new(int,int);
	c_DisplayMode* m_new2();
	void mark();
};
class c_Map : public Object{
	public:
	c_Node* m_root;
	c_Map();
	c_Map* m_new();
	virtual int p_Compare(int,int)=0;
	c_Node* p_FindNode(int);
	bool p_Contains(int);
	int p_RotateLeft(c_Node*);
	int p_RotateRight(c_Node*);
	int p_InsertFixup(c_Node*);
	bool p_Set(int,c_DisplayMode*);
	bool p_Insert(int,c_DisplayMode*);
	void mark();
};
class c_IntMap : public c_Map{
	public:
	c_IntMap();
	c_IntMap* m_new();
	int p_Compare(int,int);
	void mark();
};
class c_Stack : public Object{
	public:
	Array<c_DisplayMode* > m_data;
	int m_length;
	c_Stack();
	c_Stack* m_new();
	c_Stack* m_new2(Array<c_DisplayMode* >);
	void p_Push(c_DisplayMode*);
	void p_Push2(Array<c_DisplayMode* >,int,int);
	void p_Push3(Array<c_DisplayMode* >,int);
	Array<c_DisplayMode* > p_ToArray();
	void mark();
};
class c_Node : public Object{
	public:
	int m_key;
	c_Node* m_right;
	c_Node* m_left;
	c_DisplayMode* m_value;
	int m_color;
	c_Node* m_parent;
	c_Node();
	c_Node* m_new(int,c_DisplayMode*,int,c_Node*);
	c_Node* m_new2();
	void mark();
};
extern Array<c_DisplayMode* > bb_app__displayModes;
extern c_DisplayMode* bb_app__desktopMode;
int bb_app_DeviceWidth();
int bb_app_DeviceHeight();
void bb_app_EnumDisplayModes();
extern gxtkGraphics* bb_graphics_renderDevice;
int bb_graphics_SetMatrix(Float,Float,Float,Float,Float,Float);
int bb_graphics_SetMatrix2(Array<Float >);
int bb_graphics_SetColor(Float,Float,Float);
int bb_graphics_SetAlpha(Float);
int bb_graphics_SetBlend(int);
int bb_graphics_SetScissor(Float,Float,Float,Float);
int bb_graphics_BeginRender();
int bb_graphics_EndRender();
class c_BBGameEvent : public Object{
	public:
	c_BBGameEvent();
	void mark();
};
void bb_app_EndApp();
class c_VirtualDisplay : public Object{
	public:
	Float m_vwidth;
	Float m_vheight;
	Float m_vzoom;
	Float m_lastvzoom;
	Float m_vratio;
	Float m_multi;
	int m_lastdevicewidth;
	int m_lastdeviceheight;
	int m_device_changed;
	Float m_fdw;
	Float m_fdh;
	Float m_dratio;
	Float m_heightborder;
	Float m_widthborder;
	int m_zoom_changed;
	Float m_realx;
	Float m_realy;
	Float m_offx;
	Float m_offy;
	Float m_sx;
	Float m_sw;
	Float m_sy;
	Float m_sh;
	Float m_scaledw;
	Float m_scaledh;
	Float m_vxoff;
	Float m_vyoff;
	c_VirtualDisplay();
	static c_VirtualDisplay* m_Display;
	c_VirtualDisplay* m_new(int,int,Float);
	c_VirtualDisplay* m_new2();
	Float p_VTouchX(int,bool);
	Float p_VTouchY(int,bool);
	int p_UpdateVirtualDisplay(bool,bool);
	void mark();
};
int bb_autofit_SetVirtualDisplay(int,int,Float);
class c_Font : public virtual gc_interface{
	public:
};
class c_BitmapFont : public Object,public virtual c_Font{
	public:
	Array<c_BitMapChar* > m_borderChars;
	Array<c_BitMapChar* > m_faceChars;
	Array<c_BitMapChar* > m_shadowChars;
	Array<c_Image* > m_packedImages;
	bool m__drawShadow;
	c_DrawingPoint* m__kerning;
	bool m__drawBorder;
	c_BitmapFont();
	int p_LoadPacked(String,String,bool);
	int p_LoadFontData(String,String,bool);
	c_BitmapFont* m_new(String,bool);
	c_BitmapFont* m_new2(String);
	c_BitmapFont* m_new3();
	bool p_DrawShadow();
	int p_DrawShadow2(bool);
	c_DrawingPoint* p_Kerning();
	void p_Kerning2(c_DrawingPoint*);
	Float p_GetTxtWidth(String,int,int,bool);
	Float p_GetTxtWidth2(String);
	int p_DrawCharsText(String,Float,Float,Array<c_BitMapChar* >,int,int,int);
	int p_DrawCharsText2(String,Float,Float,int,int,int,int);
	bool p_DrawBorder();
	int p_DrawBorder2(bool);
	int p_DrawText(String,Float,Float,int,int,int);
	int p_DrawText2(String,Float,Float);
	int p_DrawText3(String,Float,Float,int);
	void mark();
};
String bb_app_LoadString(String);
class c_BitMapChar : public Object{
	public:
	int m_packedFontIndex;
	c_DrawingPoint* m_packedPosition;
	c_DrawingPoint* m_packedSize;
	c_BitMapCharMetrics* m_drawingMetrics;
	c_Image* m_image;
	String m_imageResourceName;
	String m_imageResourceNameBackup;
	c_BitMapChar();
	c_BitMapChar* m_new();
	int p_SetImageResourceName(String);
	bool p_CharImageLoaded();
	int p_LoadCharImage();
	void mark();
};
class c_DrawingPoint : public Object{
	public:
	Float m_x;
	Float m_y;
	c_DrawingPoint();
	c_DrawingPoint* m_new(Float,Float);
	c_DrawingPoint* m_new2();
	void mark();
};
class c_BitMapCharMetrics : public Object{
	public:
	c_DrawingPoint* m_drawingOffset;
	c_DrawingPoint* m_drawingSize;
	Float m_drawingWidth;
	c_BitMapCharMetrics();
	c_BitMapCharMetrics* m_new();
	void mark();
};
class c_Box2D_World : public Object{
	public:
	c_b2World* m_world;
	int m_velocityIterations;
	int m_positionIterations;
	Float m_timeStep;
	Float m_scale;
	c_List* m_entityList;
	bool m_debug;
	c_b2DebugDraw* m_dbgDraw;
	bool m_debugVisible;
	c_Box2D_World();
	c_Box2D_World* m_new(Float,Float,Float,bool);
	int p_KillEntity(c_Entity*);
	void p_Clear();
	c_Entity* p_CreateMultiPolygon(Float,Float,c_List3*,bool,Float);
	c_Entity* p_CreateImageBox(c_Image*,Float,Float,bool,Float,Float,Float,bool);
	void p_Update();
	void p_Render();
	void mark();
};
class c_b2World : public Object{
	public:
	c_b2DestructionListener* m_m_destructionListener;
	c_b2DebugDraw* m_m_debugDraw;
	c_b2Body* m_m_bodyList;
	c_b2Contact* m_m_contactList;
	c_b2Joint* m_m_jointList;
	c_b2Controller* m_m_controllerList;
	int m_m_bodyCount;
	int m_m_contactCount;
	int m_m_jointCount;
	int m_m_controllerCount;
	bool m_m_allowSleep;
	c_b2Vec2* m_m_gravity;
	Float m_m_inv_dt0;
	c_b2ContactManager* m_m_contactManager;
	int m_m_flags;
	c_b2Body* m_m_groundBody;
	c_b2Island* m_m_island;
	c_b2ContactSolver* m_m_contactSolver;
	int m_stackCapacity;
	Array<c_b2Body* > m_s_stack;
	c_b2World();
	static bool m_m_warmStarting;
	static bool m_m_continuousPhysics;
	bool p_IsLocked();
	c_b2Body* p_CreateBody(c_b2BodyDef*);
	c_b2World* m_new(c_b2Vec2*,bool);
	c_b2World* m_new2();
	void p_SetGravity(c_b2Vec2*);
	void p_SetDebugDraw(c_b2DebugDraw*);
	void p_DestroyJoint(c_b2Joint*);
	void p_DestroyBody(c_b2Body*);
	void p_SetContactListener(c_b2ContactListenerInterface*);
	static c_b2TimeStep* m_s_timestep2;
	void p_Solve(c_b2TimeStep*);
	static Array<c_b2Body* > m_s_queue;
	static c_b2Sweep* m_s_backupA;
	static c_b2Sweep* m_s_backupB;
	static c_b2TimeStep* m_s_timestep;
	void p_SolveTOI(c_b2TimeStep*);
	void p_TimeStep(Float,int,int);
	void p_ClearForces();
	void p_DrawShape(c_b2Shape*,c_b2Transform*,c_b2Color*);
	static c_b2Color* m_s_jointColor;
	void p_DrawJoint(c_b2Joint*);
	static c_b2Transform* m_s_xf;
	void p_DrawDebugData();
	void mark();
};
class c_b2Vec2 : public Object{
	public:
	Float m_x;
	Float m_y;
	c_b2Vec2();
	c_b2Vec2* m_new(Float,Float);
	void p_Set2(Float,Float);
	void p_SetV(c_b2Vec2*);
	void p_SetZero();
	Float p_LengthSquared();
	Float p_Normalize();
	static c_b2Vec2* m_Make(Float,Float);
	c_b2Vec2* p_Copy();
	Float p_Length();
	void p_GetNegative(c_b2Vec2*);
	void p_Multiply(Float);
	void p_NegativeSelf();
	void p_Add(c_b2Vec2*);
	void mark();
};
class c_b2DestructionListener : public Object{
	public:
	c_b2DestructionListener();
	void p_SayGoodbyeJoint(c_b2Joint*);
	void p_SayGoodbyeFixture(c_b2Fixture*);
	void mark();
};
class c_b2DebugDraw : public Object{
	public:
	int m_m_drawFlags;
	Float m_m_drawScale;
	Float m_m_fillAlpha;
	Float m_m_lineThickness;
	Float m_m_alpha;
	Float m_m_xformScale;
	c_b2DebugDraw();
	c_b2DebugDraw* m_new();
	void p_SetDrawScale(Float);
	void p_SetFillAlpha(Float);
	void p_SetLineThickness(Float);
	static int m_e_shapeBit;
	static int m_e_jointBit;
	void p_SetFlags(int);
	void p_Clear();
	int p_GetFlags();
	void p_SetAlpha(Float);
	void p_DrawCircle(c_b2Vec2*,Float,c_b2Color*);
	void p_DrawSolidCircle(c_b2Vec2*,Float,c_b2Vec2*,c_b2Color*);
	void p_DrawPolygon(Array<c_b2Vec2* >,int,c_b2Color*);
	void p_DrawSolidPolygon(Array<c_b2Vec2* >,int,c_b2Color*);
	void p_DrawSegment(c_b2Vec2*,c_b2Vec2*,c_b2Color*);
	static int m_e_controllerBit;
	static int m_e_pairBit;
	static int m_e_aabbBit;
	static int m_e_centerOfMassBit;
	void p_DrawTransform(c_b2Transform*);
	void mark();
};
class c_b2Body : public Object{
	public:
	int m_m_flags;
	c_b2World* m_m_world;
	c_b2Transform* m_m_xf;
	c_b2Sweep* m_m_sweep;
	c_b2JointEdge* m_m_jointList;
	c_b2ControllerEdge* m_m_controllerList;
	c_b2ContactEdge* m_m_contactList;
	int m_m_controllerCount;
	c_b2Body* m_m_prev;
	c_b2Body* m_m_next;
	c_b2Vec2* m_m_linearVelocity;
	Float m_m_angularVelocity;
	Float m_m_linearDamping;
	Float m_m_angularDamping;
	c_b2Vec2* m_m_force;
	Float m_m_torque;
	Float m_m_sleepTime;
	int m_m_type;
	Float m_m_mass;
	Float m_m_invMass;
	Float m_m_I;
	Float m_m_invI;
	Float m_m_inertiaScale;
	Object* m_m_userData;
	c_b2Fixture* m_m_fixtureList;
	int m_m_fixtureCount;
	int m_m_islandIndex;
	c_b2Body();
	c_b2Body* m_new(c_b2BodyDef*,c_b2World*);
	c_b2Body* m_new2();
	void p_SetAwake(bool);
	c_b2ContactEdge* p_GetContactList();
	void p_ResetMassData();
	c_b2Fixture* p_CreateFixture(c_b2FixtureDef*);
	Float p_GetAngularVelocity();
	void p_SetAngularVelocity(Float);
	void p_SetLinearVelocity(c_b2Vec2*);
	c_b2Vec2* p_GetPosition();
	void p_SetPositionAndAngle(c_b2Vec2*,Float);
	void p_SetAngle(Float);
	Float p_GetAngle();
	void p_SetPosition(c_b2Vec2*);
	bool p_IsAwake();
	bool p_ShouldCollide(c_b2Body*);
	c_b2Transform* p_GetTransform();
	int p_GetType();
	bool p_IsBullet();
	void p_SynchronizeTransform();
	static c_b2Transform* m_s_xf1;
	void p_SynchronizeFixtures();
	void p_Advance(Float);
	bool p_IsActive();
	c_b2Fixture* p_GetFixtureList();
	c_b2Body* p_GetNext();
	c_b2Vec2* p_GetWorldCenter();
	void p_GetWorldPoint(c_b2Vec2*,c_b2Vec2*);
	void mark();
};
class c_b2Contact : public Object{
	public:
	int m_m_flags;
	c_b2Fixture* m_m_fixtureA;
	c_b2Fixture* m_m_fixtureB;
	c_b2Contact* m_m_prev;
	c_b2Contact* m_m_next;
	c_b2ContactEdge* m_m_nodeA;
	c_b2ContactEdge* m_m_nodeB;
	c_b2Manifold* m_m_manifold;
	bool m_m_swapped;
	c_b2Manifold* m_m_oldManifold;
	Float m_m_toi;
	c_b2Contact();
	void p_FlagForFiltering();
	c_b2Fixture* p_GetFixtureA();
	c_b2Fixture* p_GetFixtureB();
	bool p_IsTouching();
	c_b2Contact* p_GetNext();
	virtual void p_Evaluate();
	void p_Update2(c_b2ContactListenerInterface*);
	c_b2Manifold* p_GetManifold();
	static c_b2TOIInput* m_s_input;
	Float p_ComputeTOI(c_b2Sweep*,c_b2Sweep*);
	virtual void p_Reset(c_b2Fixture*,c_b2Fixture*);
	c_b2Contact* m_new();
	void mark();
};
class c_b2Joint : public Object{
	public:
	bool m_m_collideConnected;
	c_b2Joint* m_m_prev;
	c_b2Joint* m_m_next;
	c_b2Body* m_m_bodyA;
	c_b2Body* m_m_bodyB;
	c_b2JointEdge* m_m_edgeA;
	c_b2JointEdge* m_m_edgeB;
	bool m_m_islandFlag;
	int m_m_type;
	c_b2Joint();
	static void m_Destroy(c_b2Joint*,Object*);
	virtual void p_InitVelocityConstraints(c_b2TimeStep*);
	virtual void p_SolveVelocityConstraints(c_b2TimeStep*);
	void p_FinalizeVelocityConstraints();
	virtual bool p_SolvePositionConstraints(Float);
	c_b2Body* p_GetBodyA();
	c_b2Body* p_GetBodyB();
	virtual void p_GetAnchorA(c_b2Vec2*);
	virtual void p_GetAnchorB(c_b2Vec2*);
	void mark();
};
class c_b2Controller : public Object{
	public:
	c_b2ControllerEdge* m_m_bodyList;
	int m_m_bodyCount;
	c_b2Controller* m_m_next;
	c_b2Controller();
	void p_RemoveBody(c_b2Body*);
	void p_TimeStep2(c_b2TimeStep*);
	void p_Draw(c_b2DebugDraw*);
	void mark();
};
class c_b2ContactManager : public Object{
	public:
	c_b2World* m_m_world;
	int m_m_contactCount;
	c_b2ContactFilter* m_m_contactFilter;
	c_b2ContactListenerInterface* m_m_contactListener;
	Object* m_m_allocator;
	c_b2ContactFactory* m_m_contactFactory;
	c_IBroadPhase* m_m_broadPhase;
	c_UpdatePairsCallback* m_pairsCallback;
	c_b2ContactManager();
	c_b2ContactManager* m_new();
	void p_Destroy(c_b2Contact*);
	void p_FindNewContacts();
	void p_Collide();
	void p_AddPair(Object*,Object*);
	void mark();
};
class c_b2ContactFilter : public Object{
	public:
	c_b2ContactFilter();
	c_b2ContactFilter* m_new();
	static c_b2ContactFilter* m_b2_defaultFilter;
	bool p_ShouldCollide2(c_b2Fixture*,c_b2Fixture*);
	void mark();
};
class c_b2ContactListenerInterface : public virtual gc_interface{
	public:
	virtual void p_EndContact(c_b2Contact*)=0;
	virtual void p_BeginContact(c_b2Contact*)=0;
	virtual void p_PreSolve(c_b2Contact*,c_b2Manifold*)=0;
	virtual void p_PostSolve(c_b2Contact*,c_b2ContactImpulse*)=0;
};
class c_b2ContactListener : public Object,public virtual c_b2ContactListenerInterface{
	public:
	c_b2ContactListener();
	c_b2ContactListener* m_new();
	static c_b2ContactListenerInterface* m_b2_defaultListener;
	virtual void p_BeginContact(c_b2Contact*);
	virtual void p_EndContact(c_b2Contact*);
	void p_PreSolve(c_b2Contact*,c_b2Manifold*);
	void p_PostSolve(c_b2Contact*,c_b2ContactImpulse*);
	void mark();
};
class c_b2ContactFactory : public Object{
	public:
	Object* m_m_allocator;
	c_FlashArray2* m_m_registers;
	c_b2ContactFactory();
	void p_AddType(c_ContactTypeFactory*,int,int);
	void p_InitializeRegisters();
	c_b2ContactFactory* m_new(Object*);
	c_b2ContactFactory* m_new2();
	void p_Destroy(c_b2Contact*);
	c_b2Contact* p_Create(c_b2Fixture*,c_b2Fixture*);
	void mark();
};
class c_b2ContactRegister : public Object{
	public:
	c_ContactTypeFactory* m_contactTypeFactory;
	bool m_primary;
	int m_poolCount;
	c_b2Contact* m_pool;
	c_b2ContactRegister();
	c_b2ContactRegister* m_new();
	void mark();
};
class c_FlashArray : public Object{
	public:
	int m_length;
	int m_arrLength;
	Array<c_b2ContactRegister* > m_arr;
	c_FlashArray();
	int p_Length();
	void p_Length2(int);
	c_FlashArray* m_new(int);
	c_FlashArray* m_new2(Array<c_b2ContactRegister* >);
	c_FlashArray* m_new3();
	void p_Set3(int,c_b2ContactRegister*);
	c_b2ContactRegister* p_Get(int);
	void mark();
};
class c_FlashArray2 : public Object{
	public:
	int m_length;
	int m_arrLength;
	Array<c_FlashArray* > m_arr;
	c_FlashArray2();
	int p_Length();
	void p_Length2(int);
	c_FlashArray2* m_new(int);
	c_FlashArray2* m_new2(Array<c_FlashArray* >);
	c_FlashArray2* m_new3();
	void p_Set4(int,c_FlashArray*);
	c_FlashArray* p_Get(int);
	void mark();
};
class c_b2Shape : public Object{
	public:
	int m_m_type;
	Float m_m_radius;
	c_b2Shape();
	int p_GetType();
	c_b2Shape* m_new();
	virtual c_b2Shape* p_Copy();
	virtual void p_ComputeAABB(c_b2AABB*,c_b2Transform*);
	virtual void p_ComputeMass(c_b2MassData*,Float);
	static bool m_TestOverlap(c_b2Shape*,c_b2Transform*,c_b2Shape*,c_b2Transform*);
	virtual void p_Set5(c_b2Shape*);
	void mark();
};
class c_ContactTypeFactory : public Object{
	public:
	c_ContactTypeFactory();
	c_ContactTypeFactory* m_new();
	virtual void p_Destroy2(c_b2Contact*,Object*)=0;
	virtual c_b2Contact* p_Create2(Object*)=0;
	void mark();
};
class c_CircleContactTypeFactory : public c_ContactTypeFactory{
	public:
	c_CircleContactTypeFactory();
	c_CircleContactTypeFactory* m_new();
	void p_Destroy2(c_b2Contact*,Object*);
	c_b2Contact* p_Create2(Object*);
	void mark();
};
class c_PolyAndCircleContactTypeFactory : public c_ContactTypeFactory{
	public:
	c_PolyAndCircleContactTypeFactory();
	c_PolyAndCircleContactTypeFactory* m_new();
	void p_Destroy2(c_b2Contact*,Object*);
	c_b2Contact* p_Create2(Object*);
	void mark();
};
class c_PolygonContactTypeFactory : public c_ContactTypeFactory{
	public:
	c_PolygonContactTypeFactory();
	c_PolygonContactTypeFactory* m_new();
	void p_Destroy2(c_b2Contact*,Object*);
	c_b2Contact* p_Create2(Object*);
	void mark();
};
class c_EdgeAndCircleContactTypeFactory : public c_ContactTypeFactory{
	public:
	c_EdgeAndCircleContactTypeFactory();
	c_EdgeAndCircleContactTypeFactory* m_new();
	void p_Destroy2(c_b2Contact*,Object*);
	c_b2Contact* p_Create2(Object*);
	void mark();
};
class c_PolyAndEdgeContactTypeFactory : public c_ContactTypeFactory{
	public:
	c_PolyAndEdgeContactTypeFactory();
	c_PolyAndEdgeContactTypeFactory* m_new();
	void p_Destroy2(c_b2Contact*,Object*);
	c_b2Contact* p_Create2(Object*);
	void mark();
};
class c_IBroadPhase : public Object{
	public:
	c_IBroadPhase();
	c_IBroadPhase* m_new();
	virtual void p_DestroyProxy(Object*)=0;
	virtual Object* p_CreateProxy(c_b2AABB*,Object*)=0;
	virtual void p_MoveProxy(Object*,c_b2AABB*,c_b2Vec2*)=0;
	virtual void p_UpdatePairs(c_UpdatePairsCallback*)=0;
	virtual bool p_TestOverlap(Object*,Object*)=0;
	virtual c_b2AABB* p_GetFatAABB(Object*)=0;
	void mark();
};
class c_b2DynamicTreeBroadPhase : public c_IBroadPhase{
	public:
	c_b2DynamicTree* m_m_tree;
	int m_m_proxyCount;
	c_FlashArray3* m_m_moveBuffer;
	c_DTQueryCallback* m_dtQueryCallBack;
	c_b2DynamicTreeBroadPhase();
	c_b2DynamicTreeBroadPhase* m_new();
	void p_BufferMove(c_b2DynamicTreeNode*);
	Object* p_CreateProxy(c_b2AABB*,Object*);
	void p_UnBufferMove(c_b2DynamicTreeNode*);
	void p_DestroyProxy(Object*);
	void p_MoveProxy(Object*,c_b2AABB*,c_b2Vec2*);
	bool p_TestOverlap(Object*,Object*);
	c_b2AABB* p_GetFatAABB(Object*);
	void p_UpdatePairs(c_UpdatePairsCallback*);
	void mark();
};
class c_UpdatePairsCallback : public Object{
	public:
	c_UpdatePairsCallback();
	c_UpdatePairsCallback* m_new();
	virtual void p_Callback(Object*,Object*)=0;
	void mark();
};
class c_CMUpdatePairsCallback : public c_UpdatePairsCallback{
	public:
	c_b2ContactManager* m_cm;
	c_CMUpdatePairsCallback();
	c_CMUpdatePairsCallback* m_new(c_b2ContactManager*);
	c_CMUpdatePairsCallback* m_new2();
	void p_Callback(Object*,Object*);
	void mark();
};
class c_b2BodyDef : public Object{
	public:
	Object* m_userData;
	c_b2Vec2* m_position;
	Float m_angle;
	c_b2Vec2* m_linearVelocity;
	Float m_angularVelocity;
	Float m_linearDamping;
	Float m_angularDamping;
	bool m_allowSleep;
	bool m_awake;
	bool m_fixedRotation;
	bool m_bullet;
	int m_type;
	bool m_active;
	Float m_inertiaScale;
	c_b2BodyDef();
	c_b2BodyDef* m_new();
	void mark();
};
class c_b2Transform : public Object{
	public:
	c_b2Vec2* m_position;
	c_b2Mat22* m_R;
	c_b2Transform();
	c_b2Transform* m_new(c_b2Vec2*,c_b2Mat22*);
	void mark();
};
class c_b2Mat22 : public Object{
	public:
	c_b2Vec2* m_col2;
	c_b2Vec2* m_col1;
	c_b2Mat22();
	c_b2Mat22* m_new();
	void p_SetM(c_b2Mat22*);
	void p_Set6(Float);
	c_b2Mat22* p_GetInverse(c_b2Mat22*);
	void mark();
};
class c_b2Sweep : public Object{
	public:
	c_b2Vec2* m_localCenter;
	Float m_t0;
	Float m_a0;
	Float m_a;
	c_b2Vec2* m_c;
	c_b2Vec2* m_c0;
	c_b2Sweep();
	c_b2Sweep* m_new();
	void p_Advance(Float);
	void p_GetTransform2(c_b2Transform*,Float);
	void p_Set7(c_b2Sweep*);
	void mark();
};
class c_b2JointEdge : public Object{
	public:
	c_b2JointEdge* m_nextItem;
	c_b2Joint* m_joint;
	c_b2JointEdge* m_prevItem;
	c_b2Body* m_other;
	c_b2JointEdge();
	c_b2JointEdge* m_new();
	void mark();
};
class c_b2ControllerEdge : public Object{
	public:
	c_b2ControllerEdge* m_nextController;
	c_b2Controller* m_controller;
	c_b2ControllerEdge* m_prevBody;
	c_b2ControllerEdge* m_nextBody;
	c_b2ControllerEdge* m_prevController;
	c_b2ControllerEdge();
	void mark();
};
class c_b2ContactEdge : public Object{
	public:
	c_b2Body* m_other;
	c_b2Contact* m_contact;
	c_b2ContactEdge* m_nextItem;
	c_b2ContactEdge* m_prevItem;
	c_b2ContactEdge();
	c_b2ContactEdge* m_new();
	void mark();
};
class c_b2Fixture : public Object{
	public:
	c_b2Body* m_m_body;
	c_b2Shape* m_m_shape;
	c_b2Fixture* m_m_next;
	Object* m_m_proxy;
	c_b2AABB* m_m_aabb;
	Object* m_m_userData;
	Float m_m_density;
	Float m_m_friction;
	Float m_m_restitution;
	c_b2FilterData* m_m_filter;
	bool m_m_isSensor;
	c_b2Fixture();
	c_b2Body* p_GetBody();
	int p_GetType();
	void p_DestroyProxy2(c_IBroadPhase*);
	void p_Destroy3();
	c_b2Fixture* m_new();
	void p_Create3(c_b2Body*,c_b2Transform*,c_b2FixtureDef*);
	void p_CreateProxy2(c_IBroadPhase*,c_b2Transform*);
	c_b2MassData* p_GetMassData(c_b2MassData*);
	static c_b2AABB* m_tmpAABB1;
	static c_b2AABB* m_tmpAABB2;
	static c_b2Vec2* m_tmpVec;
	void p_Synchronize(c_IBroadPhase*,c_b2Transform*,c_b2Transform*);
	c_b2FilterData* p_GetFilterData();
	c_b2Shape* p_GetShape();
	Float p_GetFriction();
	Float p_GetRestitution();
	c_b2AABB* p_GetAABB();
	c_b2Fixture* p_GetNext();
	bool p_IsSensor();
	void mark();
};
class c_Entity : public Object{
	public:
	c_b2Body* m_body;
	c_b2PolygonShape* m_bodyShape;
	c_b2BodyDef* m_bodyDef;
	c_b2CircleShape* m_bodyShapeCircle;
	c_b2FixtureDef* m_fixtureDef;
	c_Image* m_img;
	c_b2World* m_world;
	Float m_scale;
	int m_frame;
	c_Entity();
	c_Entity* m_new();
	void p_CreateMultiPolygon2(c_b2World*,Float,Float,c_List3*,Float,bool,Float);
	void p_CreateBox(c_b2World*,Float,Float,Float,Float,Float,Float,Float,Float,bool,bool);
	void p_CreateImageBox2(c_b2World*,c_Image*,Float,Float,Float,Float,Float,Float,bool,bool);
	void p_Kill();
	int p_RadToDeg(Float);
	void p_Draw2(Float,Float);
	void mark();
};
class c_List : public Object{
	public:
	c_Node2* m__head;
	c_List();
	c_List* m_new();
	c_Node2* p_AddLast(c_Entity*);
	c_List* m_new2(Array<c_Entity* >);
	c_Enumerator* p_ObjectEnumerator();
	int p_Count();
	void mark();
};
class c_Node2 : public Object{
	public:
	c_Node2* m__succ;
	c_Node2* m__pred;
	c_Entity* m__data;
	c_Node2();
	c_Node2* m_new(c_Node2*,c_Node2*,c_Entity*);
	c_Node2* m_new2();
	void mark();
};
class c_HeadNode : public c_Node2{
	public:
	c_HeadNode();
	c_HeadNode* m_new();
	void mark();
};
class c_Barrier : public Object{
	public:
	c_Entity* m_ent;
	Float m_x;
	Float m_y;
	Float m_startX;
	Float m_startY;
	Float m_endX;
	Float m_endY;
	int m_direction;
	Float m_speed;
	c_Barrier();
	c_Barrier* m_new(c_Box2D_World*,c_Image*,Float,Float,Float,Float,Float,Float,int,Float,bool);
	c_Barrier* m_new2();
	void p_Update();
	void mark();
};
class c_List2 : public Object{
	public:
	c_Node3* m__head;
	c_List2();
	c_List2* m_new();
	c_Node3* p_AddLast2(c_Barrier*);
	c_List2* m_new2(Array<c_Barrier* >);
	c_Enumerator3* p_ObjectEnumerator();
	int p_Clear();
	void mark();
};
class c_Node3 : public Object{
	public:
	c_Node3* m__succ;
	c_Node3* m__pred;
	c_Barrier* m__data;
	c_Node3();
	c_Node3* m_new(c_Node3*,c_Node3*,c_Barrier*);
	c_Node3* m_new2();
	void mark();
};
class c_HeadNode2 : public c_Node3{
	public:
	c_HeadNode2();
	c_HeadNode2* m_new();
	void mark();
};
Float bb_input_TouchX(int);
Float bb_autofit_VDeviceWidth();
Float bb_autofit_VTouchX(int,bool);
Float bb_input_TouchY(int);
Float bb_autofit_VDeviceHeight();
Float bb_autofit_VTouchY(int,bool);
class c_GameData : public Object{
	public:
	Array<c_StageData* > m_stage;
	c_GameData();
	c_GameData* m_new();
	String p_SaveString(int,int);
	void p_LoadString(String);
	void p_CompleteLevel(int,int,Float);
	void mark();
};
class c_StageData : public Object{
	public:
	int m_ID;
	bool m_unlocked;
	Array<c_LevelData* > m_level;
	c_StageData();
	c_StageData* m_new(int,bool);
	c_StageData* m_new2();
	void mark();
};
class c_LevelData : public Object{
	public:
	int m_ID;
	bool m_unlocked;
	int m_starsEarned;
	Float m_bestTime;
	c_LevelData();
	c_LevelData* m_new(int,bool,int,Float);
	c_LevelData* m_new2();
	void mark();
};
String bb_app_LoadState();
extern int bb_Rebound_currentVersionCode;
void bb_app_SaveState(String);
extern int bb_Rebound_versionCode;
class c_Enumerator : public Object{
	public:
	c_List* m__list;
	c_Node2* m__curr;
	c_Enumerator();
	c_Enumerator* m_new(c_List*);
	c_Enumerator* m_new2();
	bool p_HasNext();
	c_Entity* p_NextObject();
	void mark();
};
class c_b2Manifold : public Object{
	public:
	Array<c_b2ManifoldPoint* > m_m_points;
	c_b2Vec2* m_m_localPlaneNormal;
	c_b2Vec2* m_m_localPoint;
	int m_m_pointCount;
	int m_m_type;
	c_b2Manifold();
	c_b2Manifold* m_new();
	void mark();
};
class c_b2ManifoldPoint : public Object{
	public:
	c_b2Vec2* m_m_localPoint;
	Float m_m_normalImpulse;
	Float m_m_tangentImpulse;
	c_b2ContactID* m_m_id;
	c_b2ManifoldPoint();
	void p_Reset2();
	c_b2ManifoldPoint* m_new();
	void mark();
};
class c_b2Settings : public Object{
	public:
	c_b2Settings();
	static void m_B2Assert(bool);
	static Float m_B2MixFriction(Float,Float);
	static Float m_B2MixRestitution(Float,Float);
	void mark();
};
class c_b2ContactID : public Object{
	public:
	c_Features* m_features;
	int m__key;
	c_b2ContactID();
	c_b2ContactID* m_new();
	int p_Key();
	void p_Key2(int);
	void p_Set8(c_b2ContactID*);
	void mark();
};
class c_Features : public Object{
	public:
	c_b2ContactID* m__m_id;
	int m__referenceEdge;
	int m__incidentEdge;
	int m__incidentVertex;
	int m__flip;
	c_Features();
	c_Features* m_new();
	int p_ReferenceEdge();
	void p_ReferenceEdge2(int);
	int p_IncidentEdge();
	void p_IncidentEdge2(int);
	int p_IncidentVertex();
	void p_IncidentVertex2(int);
	int p_Flip();
	void p_Flip2(int);
	void mark();
};
class c_b2PolygonShape : public c_b2Shape{
	public:
	c_b2Vec2* m_m_centroid;
	Array<c_b2Vec2* > m_m_vertices;
	Array<Float > m_m_depths;
	Array<c_b2Vec2* > m_m_normals;
	int m_m_vertexCount;
	c_b2PolygonShape();
	void p_Reserve(int);
	c_b2PolygonShape* m_new();
	static c_b2Vec2* m_ComputeCentroid(Array<c_b2Vec2* >,int);
	void p_SetAsArray(Array<c_b2Vec2* >,Float);
	void p_SetAsBox(Float,Float);
	int p_GetVertexCount();
	Array<c_b2Vec2* > p_GetVertices();
	void p_Set5(c_b2Shape*);
	c_b2Shape* p_Copy();
	void p_ComputeAABB(c_b2AABB*,c_b2Transform*);
	void p_ComputeMass(c_b2MassData*,Float);
	void mark();
};
class c_b2CircleShape : public c_b2Shape{
	public:
	c_b2Vec2* m_m_p;
	c_b2CircleShape();
	c_b2CircleShape* m_new(Float);
	c_b2Shape* p_Copy();
	void p_Set5(c_b2Shape*);
	void p_ComputeAABB(c_b2AABB*,c_b2Transform*);
	void p_ComputeMass(c_b2MassData*,Float);
	void mark();
};
class c_b2FixtureDef : public Object{
	public:
	c_b2Shape* m_shape;
	Object* m_userData;
	Float m_friction;
	Float m_restitution;
	Float m_density;
	c_b2FilterData* m_filter;
	bool m_isSensor;
	c_b2FixtureDef();
	c_b2FixtureDef* m_new();
	void mark();
};
class c_Polygon : public Object{
	public:
	Array<c_b2Vec2* > m_vertices;
	int m_count;
	c_Polygon();
	c_Polygon* m_new(Array<c_b2Vec2* >,int);
	c_Polygon* m_new2();
	void mark();
};
class c_List3 : public Object{
	public:
	c_Node4* m__head;
	c_List3();
	c_List3* m_new();
	c_Node4* p_AddLast3(c_Polygon*);
	c_List3* m_new2(Array<c_Polygon* >);
	c_Enumerator2* p_ObjectEnumerator();
	void mark();
};
class c_Node4 : public Object{
	public:
	c_Node4* m__succ;
	c_Node4* m__pred;
	c_Polygon* m__data;
	c_Node4();
	c_Node4* m_new(c_Node4*,c_Node4*,c_Polygon*);
	c_Node4* m_new2();
	void mark();
};
class c_HeadNode3 : public c_Node4{
	public:
	c_HeadNode3();
	c_HeadNode3* m_new();
	void mark();
};
class c_Enumerator2 : public Object{
	public:
	c_List3* m__list;
	c_Node4* m__curr;
	c_Enumerator2();
	c_Enumerator2* m_new(c_List3*);
	c_Enumerator2* m_new2();
	bool p_HasNext();
	c_Polygon* p_NextObject();
	void mark();
};
class c_b2Math : public Object{
	public:
	c_b2Math();
	static void m_SubtractVV(c_b2Vec2*,c_b2Vec2*,c_b2Vec2*);
	static void m_CrossVF(c_b2Vec2*,Float,c_b2Vec2*);
	static void m_MulMV(c_b2Mat22*,c_b2Vec2*,c_b2Vec2*);
	static void m_MulX(c_b2Transform*,c_b2Vec2*,c_b2Vec2*);
	static Float m_Min(Float,Float);
	static Float m_Max(Float,Float);
	static Float m_CrossVV(c_b2Vec2*,c_b2Vec2*);
	static Float m_Dot(c_b2Vec2*,c_b2Vec2*);
	static void m_CrossFV(Float,c_b2Vec2*,c_b2Vec2*);
	static void m_MulTMV(c_b2Mat22*,c_b2Vec2*,c_b2Vec2*);
	static Float m_Clamp(Float,Float,Float);
	static void m_AddVV(c_b2Vec2*,c_b2Vec2*,c_b2Vec2*);
	static Float m_Abs(Float);
	void mark();
};
class c_Constants : public Object{
	public:
	c_Constants();
	void mark();
};
class c_b2FilterData : public Object{
	public:
	int m_categoryBits;
	int m_maskBits;
	int m_groupIndex;
	c_b2FilterData();
	c_b2FilterData* m_new();
	c_b2FilterData* p_Copy();
	void mark();
};
class c_b2AABB : public Object{
	public:
	c_b2Vec2* m_lowerBound;
	c_b2Vec2* m_upperBound;
	c_b2AABB();
	c_b2AABB* m_new();
	void p_Combine(c_b2AABB*,c_b2AABB*);
	bool p_TestOverlap2(c_b2AABB*);
	void p_GetCenter(c_b2Vec2*);
	bool p_Contains2(c_b2AABB*);
	static c_b2AABB* m_StaticCombine(c_b2AABB*,c_b2AABB*);
	void mark();
};
class c_b2MassData : public Object{
	public:
	Float m_mass;
	c_b2Vec2* m_center;
	Float m_I;
	c_b2MassData();
	c_b2MassData* m_new();
	void mark();
};
extern int bb_app__updateRate;
void bb_app_SetUpdateRate(int);
void bb_app_ShowMouse();
int bb_audio_PlayMusic(String,int);
int bb_audio_PauseMusic();
int bb_input_KeyDown(int);
Float bb_input_JoyZ(int,int);
int bb_input_JoyDown(int,int);
int bb_input_JoyHit(int,int);
int bb_app_Millisecs();
class c_Enumerator3 : public Object{
	public:
	c_List2* m__list;
	c_Node3* m__curr;
	c_Enumerator3();
	c_Enumerator3* m_new(c_List2*);
	c_Enumerator3* m_new2();
	bool p_HasNext();
	c_Barrier* p_NextObject();
	void mark();
};
c_List3* bb_Rebound_CreateCross1();
class c_StringObject : public Object{
	public:
	String m_value;
	c_StringObject();
	c_StringObject* m_new(int);
	c_StringObject* m_new2(Float);
	c_StringObject* m_new3(String);
	c_StringObject* m_new4();
	void mark();
};
class c_SensorContactListener : public c_b2ContactListener{
	public:
	c_b2Body* m_sensor;
	c_b2Body* m_player;
	c_SensorContactListener();
	c_SensorContactListener* m_new(c_b2Body*,c_b2Body*);
	c_SensorContactListener* m_new2();
	void p_BeginContact(c_b2Contact*);
	void p_EndContact(c_b2Contact*);
	void mark();
};
int bb_input_TouchHit(int);
int bb_input_KeyHit(int);
int bb_Rebound_aStars(Float,int,Float);
int bb_Rebound_AssignStars(int,int,Float);
class c_b2TimeStep : public Object{
	public:
	Float m_dt;
	int m_velocityIterations;
	int m_positionIterations;
	Float m_inv_dt;
	Float m_dtRatio;
	bool m_warmStarting;
	c_b2TimeStep();
	c_b2TimeStep* m_new();
	void p_Set9(c_b2TimeStep*);
	void mark();
};
class c_b2DistanceInput : public Object{
	public:
	c_b2DistanceProxy* m_proxyA;
	c_b2DistanceProxy* m_proxyB;
	c_b2Transform* m_transformA;
	c_b2Transform* m_transformB;
	bool m_useRadii;
	c_b2DistanceInput();
	c_b2DistanceInput* m_new();
	void mark();
};
class c_b2DistanceProxy : public Object{
	public:
	Array<c_b2Vec2* > m_m_vertices;
	int m_m_count;
	Float m_m_radius;
	c_b2DistanceProxy();
	c_b2DistanceProxy* m_new();
	void p_Set5(c_b2Shape*);
	c_b2Vec2* p_GetVertex(int);
	Float p_GetSupport(c_b2Vec2*);
	c_b2Vec2* p_GetSupportVertex(c_b2Vec2*);
	void mark();
};
class c_b2SimplexCache : public Object{
	public:
	int m_count;
	Array<int > m_indexA;
	Array<int > m_indexB;
	Float m_metric;
	c_b2SimplexCache();
	c_b2SimplexCache* m_new();
	void mark();
};
class c_b2DistanceOutput : public Object{
	public:
	c_b2Vec2* m_pointA;
	c_b2Vec2* m_pointB;
	Float m_distance;
	int m_iterations;
	c_b2DistanceOutput();
	c_b2DistanceOutput* m_new();
	void mark();
};
class c_b2Distance : public Object{
	public:
	c_b2Distance();
	static int m_b2_gjkCalls;
	static c_b2Simplex* m_s_simplex;
	static Array<int > m_s_saveA;
	static Array<int > m_s_saveB;
	static c_b2Vec2* m_tmpVec1;
	static c_b2Vec2* m_tmpVec2;
	static int m_b2_gjkIters;
	static int m_b2_gjkMaxIters;
	static void m_Distance(c_b2DistanceOutput*,c_b2SimplexCache*,c_b2DistanceInput*);
	void mark();
};
class c_b2Simplex : public Object{
	public:
	c_b2SimplexVertex* m_m_v1;
	Array<c_b2SimplexVertex* > m_m_vertices;
	c_b2SimplexVertex* m_m_v2;
	c_b2SimplexVertex* m_m_v3;
	int m_m_count;
	c_b2Simplex();
	c_b2Simplex* m_new();
	static c_b2Vec2* m_tmpVec1;
	static c_b2Vec2* m_tmpVec2;
	Float p_GetMetric();
	void p_ReadCache(c_b2SimplexCache*,c_b2DistanceProxy*,c_b2Transform*,c_b2DistanceProxy*,c_b2Transform*);
	void p_GetClosestPoint(c_b2Vec2*);
	void p_Solve2();
	static c_b2Vec2* m_tmpVec3;
	void p_Solve3();
	void p_GetSearchDirection(c_b2Vec2*);
	void p_GetWitnessPoints(c_b2Vec2*,c_b2Vec2*);
	void p_WriteCache(c_b2SimplexCache*);
	void mark();
};
class c_b2SimplexVertex : public Object{
	public:
	int m_indexA;
	int m_indexB;
	c_b2Vec2* m_wA;
	c_b2Vec2* m_wB;
	c_b2Vec2* m_w;
	Float m_a;
	c_b2SimplexVertex();
	c_b2SimplexVertex* m_new();
	void p_Set10(c_b2SimplexVertex*);
	void mark();
};
class c_b2Island : public Object{
	public:
	int m_m_bodyCapacity;
	Array<c_b2Body* > m_m_bodies;
	int m_m_contactCapacity;
	Array<c_b2Contact* > m_m_contacts;
	int m_m_jointCapacity;
	Array<c_b2Joint* > m_m_joints;
	int m_m_bodyCount;
	int m_m_contactCount;
	int m_m_jointCount;
	Object* m_m_allocator;
	c_b2ContactListenerInterface* m_m_listener;
	c_b2ContactSolver* m_m_contactSolver;
	c_b2Island();
	c_b2Island* m_new();
	void p_Initialize(int,int,int,Object*,c_b2ContactListenerInterface*,c_b2ContactSolver*);
	void p_Clear();
	void p_AddBody(c_b2Body*);
	void p_AddContact(c_b2Contact*);
	void p_AddJoint(c_b2Joint*);
	static c_b2ContactImpulse* m_s_impulse;
	void p_Report(Array<c_b2ContactConstraint* >);
	void p_Solve4(c_b2TimeStep*,c_b2Vec2*,bool);
	void p_SolveTOI(c_b2TimeStep*);
	void mark();
};
class c_b2ContactSolver : public Object{
	public:
	int m_constraintCapacity;
	Array<c_b2ContactConstraint* > m_m_constraints;
	c_b2TimeStep* m_m_step;
	Object* m_m_allocator;
	int m_m_constraintCount;
	c_b2ContactSolver();
	c_b2ContactSolver* m_new();
	static c_b2WorldManifold* m_s_worldManifold;
	void p_Initialize2(c_b2TimeStep*,Array<c_b2Contact* >,int,Object*);
	void p_InitVelocityConstraints(c_b2TimeStep*);
	void p_SolveVelocityConstraints2();
	void p_FinalizeVelocityConstraints();
	static c_b2PositionSolverManifold* m_s_psm;
	bool p_SolvePositionConstraints(Float);
	void mark();
};
class c_b2ContactConstraint : public Object{
	public:
	Array<c_b2ContactConstraintPoint* > m_points;
	c_b2Body* m_bodyA;
	c_b2Body* m_bodyB;
	c_b2Manifold* m_manifold;
	c_b2Vec2* m_normal;
	int m_pointCount;
	Float m_friction;
	Float m_restitution;
	c_b2Vec2* m_localPlaneNormal;
	c_b2Vec2* m_localPoint;
	Float m_radius;
	int m_type;
	c_b2Mat22* m_K;
	c_b2Mat22* m_normalMass;
	c_b2ContactConstraint();
	c_b2ContactConstraint* m_new();
	void mark();
};
class c_b2ContactConstraintPoint : public Object{
	public:
	Float m_normalImpulse;
	Float m_tangentImpulse;
	c_b2Vec2* m_localPoint;
	c_b2Vec2* m_rA;
	c_b2Vec2* m_rB;
	Float m_normalMass;
	Float m_equalizedMass;
	Float m_tangentMass;
	Float m_velocityBias;
	c_b2ContactConstraintPoint();
	c_b2ContactConstraintPoint* m_new();
	void mark();
};
class c_b2WorldManifold : public Object{
	public:
	Array<c_b2Vec2* > m_m_points;
	c_b2Vec2* m_m_normal;
	c_b2WorldManifold();
	c_b2WorldManifold* m_new();
	void p_Initialize3(c_b2Manifold*,c_b2Transform*,Float,c_b2Transform*,Float);
	void mark();
};
class c_b2PositionSolverManifold : public Object{
	public:
	c_b2Vec2* m_m_normal;
	Array<Float > m_m_separations;
	Array<c_b2Vec2* > m_m_points;
	c_b2PositionSolverManifold();
	c_b2PositionSolverManifold* m_new();
	void p_Initialize4(c_b2ContactConstraint*);
	void mark();
};
class c_b2ContactImpulse : public Object{
	public:
	Array<Float > m_normalImpulses;
	Array<Float > m_tangentImpulses;
	c_b2ContactImpulse();
	c_b2ContactImpulse* m_new();
	void mark();
};
class c_b2TOIInput : public Object{
	public:
	c_b2DistanceProxy* m_proxyA;
	c_b2DistanceProxy* m_proxyB;
	c_b2Sweep* m_sweepA;
	c_b2Sweep* m_sweepB;
	Float m_tolerance;
	c_b2TOIInput();
	c_b2TOIInput* m_new();
	void mark();
};
class c_b2TimeOfImpact : public Object{
	public:
	c_b2TimeOfImpact();
	static int m_b2_toiCalls;
	static c_b2SimplexCache* m_s_cache;
	static c_b2DistanceInput* m_s_distanceInput;
	static c_b2Transform* m_s_xfA;
	static c_b2Transform* m_s_xfB;
	static c_b2DistanceOutput* m_s_distanceOutput;
	static c_b2SeparationFunction* m_s_fcn;
	static int m_b2_toiRootIters;
	static int m_b2_toiMaxRootIters;
	static int m_b2_toiIters;
	static int m_b2_toiMaxIters;
	static Float m_TimeOfImpact(c_b2TOIInput*);
	void mark();
};
class c_b2SeparationFunction : public Object{
	public:
	c_b2DistanceProxy* m_m_proxyA;
	c_b2DistanceProxy* m_m_proxyB;
	c_b2Sweep* m_m_sweepA;
	c_b2Sweep* m_m_sweepB;
	int m_m_type;
	c_b2Vec2* m_m_axis;
	c_b2Vec2* m_m_localPoint;
	c_b2SeparationFunction();
	c_b2SeparationFunction* m_new();
	static c_b2Transform* m_tmpTransA;
	static c_b2Transform* m_tmpTransB;
	static c_b2Vec2* m_tmpVec1;
	static c_b2Vec2* m_tmpVec2;
	static c_b2Vec2* m_tmpVec3;
	void p_Initialize5(c_b2SimplexCache*,c_b2DistanceProxy*,c_b2Sweep*,c_b2DistanceProxy*,c_b2Sweep*,Float);
	Float p_Evaluate2(c_b2Transform*,c_b2Transform*);
	void mark();
};
class c_DoTweet : public Object{
	public:
	c_DoTweet();
	static void m_LaunchTwitter(String,String,String);
	void mark();
};
void bb_Rebound_LaunchBrowser(String,String);
void bb_app_SetDeviceWindow(int,int,int);
void bb_app_HideMouse();
int bb_math_Max(int,int);
Float bb_math_Max2(Float,Float);
int bb_math_Min(int,int);
Float bb_math_Min2(Float,Float);
int bb_graphics_Cls(Float,Float,Float);
int bb_graphics_Transform(Float,Float,Float,Float,Float,Float);
int bb_graphics_Transform2(Array<Float >);
int bb_graphics_Scale(Float,Float);
int bb_graphics_Translate(Float,Float);
int bb_autofit_UpdateVirtualDisplay(bool,bool);
int bb_graphics_DrawImage(c_Image*,Float,Float,int);
int bb_graphics_PushMatrix();
int bb_graphics_Rotate(Float);
int bb_graphics_PopMatrix();
int bb_graphics_DrawImage2(c_Image*,Float,Float,Float,Float,Float,int);
class c_eDrawAlign : public Object{
	public:
	c_eDrawAlign();
	void mark();
};
class c_eDrawMode : public Object{
	public:
	c_eDrawMode();
	void mark();
};
int bb_math_Abs(int);
Float bb_math_Abs2(Float);
int bb_graphics_DrawImageRect(c_Image*,Float,Float,int,int,int,int,int);
int bb_graphics_DrawImageRect2(c_Image*,Float,Float,int,int,int,int,Float,Float,Float,int);
class c_b2Color : public Object{
	public:
	int m__r;
	int m__g;
	int m__b;
	c_b2Color();
	c_b2Color* m_new(Float,Float,Float);
	c_b2Color* m_new2();
	void p_Set11(Float,Float,Float);
	void mark();
};
int bb_graphics_DrawCircle(Float,Float,Float);
int bb_graphics_DrawLine(Float,Float,Float,Float);
class c_b2EdgeShape : public c_b2Shape{
	public:
	c_b2Vec2* m_m_v1;
	c_b2Vec2* m_m_v2;
	c_b2EdgeShape* m_m_prevEdge;
	c_b2EdgeShape* m_m_nextEdge;
	c_b2Vec2* m_m_direction;
	Float m_m_length;
	c_b2Vec2* m_m_normal;
	c_b2Vec2* m_m_coreV1;
	c_b2Vec2* m_m_coreV2;
	c_b2Vec2* m_m_cornerDir1;
	c_b2Vec2* m_m_cornerDir2;
	c_b2EdgeShape();
	c_b2Vec2* p_GetVertex1();
	c_b2Vec2* p_GetVertex2();
	void p_ComputeAABB(c_b2AABB*,c_b2Transform*);
	void p_ComputeMass(c_b2MassData*,Float);
	c_b2EdgeShape* m_new(c_b2Vec2*,c_b2Vec2*);
	c_b2EdgeShape* m_new2();
	c_b2Shape* p_Copy();
	void mark();
};
class c_b2PulleyJoint : public c_b2Joint{
	public:
	c_b2Body* m_m_ground;
	c_b2Vec2* m_m_groundAnchor1;
	c_b2Vec2* m_m_groundAnchor2;
	c_b2Vec2* m_m_localAnchor1;
	c_b2Vec2* m_m_localAnchor2;
	c_b2Vec2* m_m_u1;
	c_b2Vec2* m_m_u2;
	Float m_m_constant;
	Float m_m_ratio;
	int m_m_state;
	Float m_m_impulse;
	Float m_m_maxLength1;
	int m_m_limitState1;
	Float m_m_limitImpulse1;
	Float m_m_maxLength2;
	int m_m_limitState2;
	Float m_m_limitImpulse2;
	Float m_m_limitMass1;
	Float m_m_limitMass2;
	Float m_m_pulleyMass;
	c_b2PulleyJoint();
	void p_GetGroundAnchorA(c_b2Vec2*);
	void p_GetGroundAnchorB(c_b2Vec2*);
	void p_GetAnchorA(c_b2Vec2*);
	void p_GetAnchorB(c_b2Vec2*);
	void p_InitVelocityConstraints(c_b2TimeStep*);
	void p_SolveVelocityConstraints(c_b2TimeStep*);
	bool p_SolvePositionConstraints(Float);
	void mark();
};
class c_b2PolyAndCircleContact : public c_b2Contact{
	public:
	c_b2PolyAndCircleContact();
	void p_Reset(c_b2Fixture*,c_b2Fixture*);
	void p_Evaluate();
	c_b2PolyAndCircleContact* m_new();
	void mark();
};
int bb_graphics_DrawRect(Float,Float,Float,Float);
class c_b2DynamicTreeNode : public Object{
	public:
	c_b2DynamicTreeNode* m_parent;
	c_b2DynamicTreeNode* m_child1;
	c_b2DynamicTreeNode* m_child2;
	int m_id;
	c_b2AABB* m_aabb;
	Object* m_userData;
	c_b2DynamicTreeNode();
	static int m_idCount;
	c_b2DynamicTreeNode* m_new();
	void mark();
};
class c_b2DynamicTree : public Object{
	public:
	c_b2DynamicTreeNode* m_m_root;
	c_b2DynamicTreeNode* m_m_freeList;
	int m_m_path;
	int m_m_insertionCount;
	Array<c_b2DynamicTreeNode* > m_nodeStack;
	c_b2DynamicTree();
	c_b2DynamicTree* m_new();
	c_b2DynamicTreeNode* p_AllocateNode();
	static c_b2Vec2* m_shared_aabbCenter;
	void p_InsertLeaf(c_b2DynamicTreeNode*);
	c_b2DynamicTreeNode* p_CreateProxy(c_b2AABB*,Object*);
	void p_FreeNode(c_b2DynamicTreeNode*);
	void p_RemoveLeaf(c_b2DynamicTreeNode*);
	void p_DestroyProxy3(c_b2DynamicTreeNode*);
	bool p_MoveProxy2(c_b2DynamicTreeNode*,c_b2AABB*,c_b2Vec2*);
	c_b2AABB* p_GetFatAABB2(c_b2DynamicTreeNode*);
	void p_Query(c_QueryCallback*,c_b2AABB*);
	Object* p_GetUserData(c_b2DynamicTreeNode*);
	void mark();
};
class c_FlashArray3 : public Object{
	public:
	int m_length;
	int m_arrLength;
	Array<c_b2DynamicTreeNode* > m_arr;
	Array<c_b2DynamicTreeNode* > m_EmptyArr;
	c_FlashArray3();
	int p_Length();
	void p_Length2(int);
	c_FlashArray3* m_new(int);
	c_FlashArray3* m_new2(Array<c_b2DynamicTreeNode* >);
	c_FlashArray3* m_new3();
	void p_Set12(int,c_b2DynamicTreeNode*);
	int p_IndexOf(c_b2DynamicTreeNode*);
	void p_Splice(int,int,Array<c_b2DynamicTreeNode* >);
	void p_Splice2(int,int,c_b2DynamicTreeNode*);
	void p_Splice3(int,int);
	Array<c_b2DynamicTreeNode* > p_BackingArray();
	void mark();
};
class c_QueryCallback : public Object{
	public:
	c_QueryCallback();
	c_QueryCallback* m_new();
	virtual bool p_Callback2(Object*)=0;
	void mark();
};
class c_TreeQueryCallback : public c_QueryCallback{
	public:
	c_TreeQueryCallback();
	c_TreeQueryCallback* m_new();
	bool p_Callback2(Object*)=0;
	void mark();
};
class c_DTQueryCallback : public c_TreeQueryCallback{
	public:
	int m_m_pairCount;
	c_b2DynamicTreeNode* m_queryProxy;
	c_FlashArray4* m_m_pairBuffer;
	c_DTQueryCallback();
	c_DTQueryCallback* m_new();
	bool p_Callback2(Object*);
	void mark();
};
class c_b2DynamicTreePair : public Object{
	public:
	c_b2DynamicTreeNode* m_proxyA;
	c_b2DynamicTreeNode* m_proxyB;
	c_b2DynamicTreePair();
	c_b2DynamicTreePair* m_new();
	void mark();
};
class c_FlashArray4 : public Object{
	public:
	int m_length;
	int m_arrLength;
	Array<c_b2DynamicTreePair* > m_arr;
	c_FlashArray4();
	int p_Length();
	void p_Length2(int);
	c_FlashArray4* m_new(int);
	c_FlashArray4* m_new2(Array<c_b2DynamicTreePair* >);
	c_FlashArray4* m_new3();
	c_b2DynamicTreePair* p_Get(int);
	void p_Set13(int,c_b2DynamicTreePair*);
	void mark();
};
class c_b2Collision : public Object{
	public:
	c_b2Collision();
	static void m_CollidePolygonAndCircle(c_b2Manifold*,c_b2PolygonShape*,c_b2Transform*,c_b2CircleShape*,c_b2Transform*);
	static void m_CollideCircles(c_b2Manifold*,c_b2CircleShape*,c_b2Transform*,c_b2CircleShape*,c_b2Transform*);
	static Array<int > m_s_edgeAO;
	static Float m_EdgeSeparation(c_b2PolygonShape*,c_b2Transform*,int,c_b2PolygonShape*,c_b2Transform*);
	static Float m_FindMaxSeparation(Array<int >,c_b2PolygonShape*,c_b2Transform*,c_b2PolygonShape*,c_b2Transform*);
	static Array<int > m_s_edgeBO;
	static Array<c_ClipVertex* > m_MakeClipPointVector();
	static Array<c_ClipVertex* > m_s_incidentEdge;
	static void m_FindIncidentEdge(Array<c_ClipVertex* >,c_b2PolygonShape*,c_b2Transform*,int,c_b2PolygonShape*,c_b2Transform*);
	static c_b2Vec2* m_s_localTangent;
	static c_b2Vec2* m_s_localNormal;
	static c_b2Vec2* m_s_planePoint;
	static c_b2Vec2* m_s_tangent;
	static c_b2Vec2* m_s_tangent2;
	static c_b2Vec2* m_s_normal;
	static c_b2Vec2* m_s_v11;
	static c_b2Vec2* m_s_v12;
	static Array<c_ClipVertex* > m_s_clipPoints1;
	static Array<c_ClipVertex* > m_s_clipPoints2;
	static int m_ClipSegmentToLine(Array<c_ClipVertex* >,Array<c_ClipVertex* >,c_b2Vec2*,Float);
	static void m_CollidePolygons(c_b2Manifold*,c_b2PolygonShape*,c_b2Transform*,c_b2PolygonShape*,c_b2Transform*);
	void mark();
};
class c_b2CircleContact : public c_b2Contact{
	public:
	c_b2CircleContact();
	c_b2CircleContact* m_new();
	void p_Reset(c_b2Fixture*,c_b2Fixture*);
	void p_Evaluate();
	void mark();
};
class c_b2PolygonContact : public c_b2Contact{
	public:
	c_b2PolygonContact();
	c_b2PolygonContact* m_new();
	void p_Reset(c_b2Fixture*,c_b2Fixture*);
	void p_Evaluate();
	void mark();
};
class c_b2EdgeAndCircleContact : public c_b2Contact{
	public:
	c_b2EdgeAndCircleContact();
	c_b2EdgeAndCircleContact* m_new();
	void p_Reset(c_b2Fixture*,c_b2Fixture*);
	void p_B2CollideEdgeAndCircle(c_b2Manifold*,c_b2EdgeShape*,c_b2Transform*,c_b2CircleShape*,c_b2Transform*);
	void p_Evaluate();
	void mark();
};
class c_b2PolyAndEdgeContact : public c_b2Contact{
	public:
	c_b2PolyAndEdgeContact();
	c_b2PolyAndEdgeContact* m_new();
	void p_Reset(c_b2Fixture*,c_b2Fixture*);
	void p_B2CollidePolyAndEdge(c_b2Manifold*,c_b2PolygonShape*,c_b2Transform*,c_b2EdgeShape*,c_b2Transform*);
	void p_Evaluate();
	void mark();
};
class c_ClipVertex : public Object{
	public:
	c_b2Vec2* m_v;
	c_b2ContactID* m_id;
	c_ClipVertex();
	c_ClipVertex* m_new();
	void p_Set14(c_ClipVertex*);
	void mark();
};
void gc_mark( BBGame *p ){}
c_App::c_App(){
}
c_App* c_App::m_new(){
	if((bb_app__app)!=0){
		bbError(String(L"App has already been created",28));
	}
	gc_assign(bb_app__app,this);
	gc_assign(bb_app__delegate,(new c_GameDelegate)->m_new());
	bb_app__game->SetDelegate(bb_app__delegate);
	return this;
}
int c_App::p_OnResize(){
	return 0;
}
int c_App::p_OnCreate(){
	return 0;
}
int c_App::p_OnSuspend(){
	return 0;
}
int c_App::p_OnResume(){
	return 0;
}
int c_App::p_OnUpdate(){
	return 0;
}
int c_App::p_OnLoading(){
	return 0;
}
int c_App::p_OnRender(){
	return 0;
}
int c_App::p_OnClose(){
	bb_app_EndApp();
	return 0;
}
int c_App::p_OnBack(){
	p_OnClose();
	return 0;
}
void c_App::mark(){
	Object::mark();
}
c_Game::c_Game(){
	m_img_player=0;
	m_img_mainMenu=0;
	m_img_stageSelect=0;
	m_img_levelComplete=0;
	m_img_level=0;
	m_img_redo=0;
	m_img_menu=0;
	m_img_exit=0;
	m_img_starFull=0;
	m_img_starFullLarge=0;
	m_img_levelH=0;
	m_img_starFullH=0;
	m_img_rightArrowH=0;
	m_img_leftArrowH=0;
	m_img_menuH1=0;
	m_img_optionsH1=0;
	m_img_playH=0;
	m_img_tutorialH=0;
	m_img_optionsH2=0;
	m_img_menuH2=0;
	m_img_menuH3=0;
	m_img_nextH=0;
	m_img_twitterH=0;
	m_img_redoH=0;
	m_img_barrierV=0;
	m_img_barrierH=0;
	m_img_cross1=0;
	m_img_cube1=0;
	m_img_options=0;
	m_img_res1H=0;
	m_img_res2H=0;
	m_img_res3H=0;
	m_img_windowedH=0;
	m_img_fullscreenH=0;
	m_img_creditsH=0;
	m_img_exitGameH=0;
	m_img_returnH=0;
	m_img_stage2=0;
	m_img_stage3=0;
	m_img_completed=0;
	m_img_androidOptions=0;
	m_img_on=0;
	m_img_off=0;
	m_fnt_stageFont=0;
	m_fnt_stageSelectFont=0;
	m_fnt_stageSelectFontH=0;
	m_fnt_72Font=0;
	m_fnt_54Font=0;
	m_fnt_timeFont=0;
	m_debug=false;
	m_world=0;
	m_tutorial=false;
	m_tutStep=0;
	m_options=false;
	m_fullscreen=false;
	m_resX=0;
	m_resY=0;
	m_credits=false;
	m_creditStep=0;
	m_level=0;
	m_stage=0;
	m_hasWon=false;
	m_levelStartTimer=0;
	m_finalTime=FLOAT(.0);
	m_barrierList=0;
	m_touchDelayStart=0;
	m_touchDelayTime=0;
	m_main_menu=false;
	m_isPlaying=false;
	m_levelComplete=false;
	m_oldSelector=0;
	m_selector=0;
	m_mX=FLOAT(.0);
	m_mY=FLOAT(.0);
	m_gamepad=false;
	m_gameProgress=0;
	m_gameData=0;
	m_musicPlaying=false;
	m_playerFriction=0;
	m_player=0;
	m_maxAngularVelocity=FLOAT(.0);
	m_exitX=FLOAT(.0);
	m_exitY=FLOAT(.0);
	m_fTime=String();
	m_tStars=0;
	m_tutTime=0;
	m_tutWait=0;
	m_gameProgressTimer=0;
	m_creditStart=0;
}
c_Game* c_Game::m_new(){
	c_App::m_new();
	return this;
}
c_Entity* c_Game::p_CreateLevel1(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(3);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-754.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,3));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(3);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-700.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,3));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel2(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-6.000)/t_scale,FLOAT(366.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-6.000)/t_scale,FLOAT(521.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-132.000)/t_scale,FLOAT(521.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-132.000)/t_scale,FLOAT(366.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(3);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-729.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,3));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(521.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(521.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(521.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(3);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-717.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,3));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(521.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel3(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-356.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-958.000)/t_scale,FLOAT(423.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-938.000)/t_scale,FLOAT(403.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-356.000)/t_scale,FLOAT(403.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(942.000)/t_scale,FLOAT(403.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(962.000)/t_scale,FLOAT(423.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(36.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(36.000)/t_scale,FLOAT(403.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(942.000)/t_scale,FLOAT(403.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(942.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(962.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(962.000)/t_scale,FLOAT(423.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-958.000)/t_scale,FLOAT(423.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-958.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-938.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-938.000)/t_scale,FLOAT(403.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(942.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-938.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-958.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(962.000)/t_scale,FLOAT(-540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel4(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-252.000)/t_scale,FLOAT(369.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-252.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-378.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-378.000)/t_scale,FLOAT(369.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(173.000)/t_scale,FLOAT(369.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(173.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(47.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(47.000)/t_scale,FLOAT(369.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(3);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-752.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,3));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(3);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-735.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,3));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel5_2(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(362.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(362.000)/t_scale,FLOAT(169.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(382.000)/t_scale,FLOAT(189.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(382.000)/t_scale,FLOAT(540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(169.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(189.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(382.000)/t_scale,FLOAT(189.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(362.000)/t_scale,FLOAT(169.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(169.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(189.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(169.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(189.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(169.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-362.000)/t_scale,FLOAT(169.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-382.000)/t_scale,FLOAT(189.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(189.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-362.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-382.000)/t_scale,FLOAT(539.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-382.000)/t_scale,FLOAT(189.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-362.000)/t_scale,FLOAT(169.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(3);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-382.000)/t_scale,FLOAT(539.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-362.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,3));
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel5(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(111.000)/t_scale,FLOAT(394.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(111.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-111.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-111.000)/t_scale,FLOAT(394.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	p_CreateLevel5_2();
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel6_2(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(246.000)/t_scale,FLOAT(-72.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(246.000)/t_scale,FLOAT(24.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-245.000)/t_scale,FLOAT(24.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-245.000)/t_scale,FLOAT(-72.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel6_3(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(246.000)/t_scale,FLOAT(368.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(246.000)/t_scale,FLOAT(640.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-245.000)/t_scale,FLOAT(640.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-245.000)/t_scale,FLOAT(368.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel6(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	p_CreateLevel6_2();
	p_CreateLevel6_3();
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel7(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-376.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(99.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-376.000)/t_scale,FLOAT(99.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(258.000)/t_scale,FLOAT(-521.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(361.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(361.000)/t_scale,FLOAT(61.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(258.000)/t_scale,FLOAT(61.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(3);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(364.000)/t_scale,FLOAT(-521.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,3));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(258.000)/t_scale,FLOAT(-521.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(361.000)/t_scale,FLOAT(-540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(99.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(3);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,3));
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel8(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-420.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(452.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-420.000)/t_scale,FLOAT(452.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(452.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(420.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(420.000)/t_scale,FLOAT(452.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(3);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(326.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,3));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(452.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(3);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(312.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,3));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(452.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(420.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-420.000)/t_scale,FLOAT(520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel9(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(6);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(344.000)/t_scale,FLOAT(119.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(344.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(324.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(157.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[4],(new c_b2Vec2)->m_new(FLOAT(137.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[5],(new c_b2Vec2)->m_new(FLOAT(137.000)/t_scale,FLOAT(119.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,6));
	t_vertices=Array<c_b2Vec2* >(6);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-138.000)/t_scale,FLOAT(119.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-138.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-158.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-325.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[4],(new c_b2Vec2)->m_new(FLOAT(-345.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[5],(new c_b2Vec2)->m_new(FLOAT(-345.000)/t_scale,FLOAT(119.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,6));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-27.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-158.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-138.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-27.000)/t_scale,FLOAT(520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(137.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(157.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-16.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-26.000)/t_scale,FLOAT(520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-325.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-345.000)/t_scale,FLOAT(520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(324.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(344.000)/t_scale,FLOAT(520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-160.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-170.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(5);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(580.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(588.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(590.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-170.000)/t_scale));
	gc_assign(t_vertices[4],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-160.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,5));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-170.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-160.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(5);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-170.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-590.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-588.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-580.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[4],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-160.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,5));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(580.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-580.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-588.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(588.000)/t_scale,FLOAT(-540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel10(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(216.000)/t_scale,FLOAT(128.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(216.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(116.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(116.000)/t_scale,FLOAT(128.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(116.000)/t_scale,FLOAT(-126.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(116.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(216.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(216.000)/t_scale,FLOAT(-126.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(516.000)/t_scale,FLOAT(-80.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(516.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(616.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(616.000)/t_scale,FLOAT(-80.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-526.000)/t_scale,FLOAT(-77.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-526.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-426.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-426.000)/t_scale,FLOAT(-77.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-426.000)/t_scale,FLOAT(77.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-426.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-526.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-526.000)/t_scale,FLOAT(77.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(616.000)/t_scale,FLOAT(77.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(616.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(516.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(516.000)/t_scale,FLOAT(77.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(414.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(414.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-426.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(394.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(413.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-426.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(3);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-426.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,3));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(3);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-426.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,3));
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel11_2(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(40.000)/t_scale,FLOAT(-154.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(40.000)/t_scale,FLOAT(155.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-40.000)/t_scale,FLOAT(155.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-40.000)/t_scale,FLOAT(-154.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel11(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-774.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-774.000)/t_scale,FLOAT(520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-756.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-773.000)/t_scale,FLOAT(520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	p_CreateLevel11_2();
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel13_2(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-207.000)/t_scale,FLOAT(-44.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-42.000)/t_scale,FLOAT(-208.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(0.000)/t_scale,FLOAT(-165.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-165.000)/t_scale,FLOAT(0.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel13_3(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(207.000)/t_scale,FLOAT(43.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(42.000)/t_scale,FLOAT(208.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(0.000)/t_scale,FLOAT(165.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(165.000)/t_scale,FLOAT(0.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel13(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-41.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-481.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-41.000)/t_scale,FLOAT(520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(3);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(60.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-481.000)/t_scale,FLOAT(520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,3));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(480.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-40.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-40.000)/t_scale,FLOAT(520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(3);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(60.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(480.000)/t_scale,FLOAT(520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,3));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(60.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-59.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-61.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(60.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(3);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-481.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-61.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,3));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(478.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-481.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(3);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-59.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(478.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,3));
	p_CreateLevel13_2();
	p_CreateLevel13_3();
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel17(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(394.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(600.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(600.000)/t_scale,FLOAT(394.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-600.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(394.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-600.000)/t_scale,FLOAT(394.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(394.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(394.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel18(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-654.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-654.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-384.000)/t_scale,FLOAT(2.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(2.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(76.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-384.000)/t_scale,FLOAT(76.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-636.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-653.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel19_2(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-717.000)/t_scale,FLOAT(396.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-149.000)/t_scale,FLOAT(396.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-149.000)/t_scale,FLOAT(474.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-717.000)/t_scale,FLOAT(474.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel19_3(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(311.000)/t_scale,FLOAT(493.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(255.000)/t_scale,FLOAT(438.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(657.000)/t_scale,FLOAT(36.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(713.000)/t_scale,FLOAT(92.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel19(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-717.000)/t_scale,FLOAT(-77.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-149.000)/t_scale,FLOAT(-77.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-149.000)/t_scale,FLOAT(1.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-717.000)/t_scale,FLOAT(1.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	p_CreateLevel19_2();
	p_CreateLevel19_3();
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel20(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(2.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(642.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(642.000)/t_scale,FLOAT(2.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(525.000)/t_scale,FLOAT(520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(2.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(525.000)/t_scale,FLOAT(2.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-250.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-250.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(2.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-218.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-249.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(2.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(3);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(642.000)/t_scale,FLOAT(520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,3));
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel22_2(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-752.000)/t_scale,FLOAT(424.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-752.000)/t_scale,FLOAT(210.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-566.000)/t_scale,FLOAT(210.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-566.000)/t_scale,FLOAT(424.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel22_3(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(566.000)/t_scale,FLOAT(424.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(566.000)/t_scale,FLOAT(210.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(752.000)/t_scale,FLOAT(210.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(752.000)/t_scale,FLOAT(424.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel22(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-93.000)/t_scale,FLOAT(423.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-93.000)/t_scale,FLOAT(209.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(93.000)/t_scale,FLOAT(209.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(93.000)/t_scale,FLOAT(423.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	p_CreateLevel22_2();
	p_CreateLevel22_3();
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
c_Entity* c_Game::p_CreateLevel23(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-324.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-307.000)/t_scale,FLOAT(-520.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(474.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(494.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(-540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-940.000)/t_scale,FLOAT(474.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-335.000)/t_scale,FLOAT(474.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-315.000)/t_scale,FLOAT(494.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-960.000)/t_scale,FLOAT(494.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-335.000)/t_scale,FLOAT(474.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-335.000)/t_scale,FLOAT(213.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-315.000)/t_scale,FLOAT(233.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-315.000)/t_scale,FLOAT(494.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-335.000)/t_scale,FLOAT(213.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-164.000)/t_scale,FLOAT(213.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-184.000)/t_scale,FLOAT(233.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-315.000)/t_scale,FLOAT(233.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-164.000)/t_scale,FLOAT(323.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-184.000)/t_scale,FLOAT(343.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-184.000)/t_scale,FLOAT(233.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-164.000)/t_scale,FLOAT(213.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-164.000)/t_scale,FLOAT(323.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(323.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(343.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(-184.000)/t_scale,FLOAT(343.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(323.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(343.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(940.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-306.000)/t_scale,FLOAT(-520.000)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-306.000)/t_scale,FLOAT(-540.000)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(960.000)/t_scale,FLOAT(-540.000)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	return this->m_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),t_pList,true,FLOAT(0.9));
}
void c_Game::p_BuildLevel(int t_l){
	m_world->p_Clear();
	if(m_stage==0){
		if(t_l==1){
			m_world->m_world->p_SetGravity((new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(1.0)));
			p_CreateLevel1();
		}else{
			if(t_l==2){
				m_world->m_world->p_SetGravity((new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(10.0)));
				p_CreateLevel1();
			}
		}
	}else{
		if(m_stage==1){
			m_world->m_world->p_SetGravity((new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(10.0)));
			if(t_l==1){
				p_CreateLevel1();
			}else{
				if(t_l==2){
					p_CreateLevel2();
				}else{
					if(t_l==3){
						p_CreateLevel3();
					}else{
						if(t_l==4){
							p_CreateLevel4();
						}else{
							if(t_l==5){
								p_CreateLevel5();
							}else{
								if(t_l==6){
									p_CreateLevel6();
								}else{
									if(t_l==7){
										p_CreateLevel7();
									}else{
										if(t_l==8){
											p_CreateLevel8();
										}
									}
								}
							}
						}
					}
				}
			}
		}else{
			if(m_stage==2){
				m_world->m_world->p_SetGravity((new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0)));
				if(t_l==1){
					p_CreateLevel9();
				}else{
					if(t_l==2){
						p_CreateLevel10();
					}else{
						if(t_l==3){
							p_CreateLevel11();
						}else{
							if(t_l==4){
								p_CreateLevel1();
							}else{
								if(t_l==5){
									p_CreateLevel13();
								}else{
									if(t_l==6){
									}else{
										if(t_l==7){
											p_CreateLevel1();
										}else{
											if(t_l==8){
												p_CreateLevel1();
											}
										}
									}
								}
							}
						}
					}
				}
			}else{
				if(m_stage==3){
					m_world->m_world->p_SetGravity((new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(10.0)));
					if(t_l==1){
						p_CreateLevel17();
					}else{
						if(t_l==2){
							p_CreateLevel18();
						}else{
							if(t_l==3){
								p_CreateLevel19();
							}else{
								if(t_l==4){
									p_CreateLevel20();
								}else{
									if(t_l==5){
										p_CreateLevel8();
									}else{
										if(t_l==6){
											p_CreateLevel22();
										}else{
											if(t_l==7){
												p_CreateLevel23();
											}else{
												if(t_l==8){
													p_CreateLevel1();
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}
int c_Game::p_OnCreate(){
	bb_autofit_SetVirtualDisplay(1920,1080,FLOAT(1.0));
	gc_assign(m_img_player,bb_graphics_LoadImage(String(L"Graphics/Player/Player.png",26),1,1));
	gc_assign(m_img_mainMenu,bb_graphics_LoadImage(String(L"Graphics/UI/Main_Menu.png",25),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_stageSelect,bb_graphics_LoadImage(String(L"Graphics/UI/Stage_Select.png",28),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_levelComplete,bb_graphics_LoadImage(String(L"Graphics/UI/Level_Complete.png",30),1,1));
	gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level12.png",27),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_redo,bb_graphics_LoadImage(String(L"Graphics/UI/Redo.png",20),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_menu,bb_graphics_LoadImage(String(L"Graphics/UI/Menu.png",20),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_exit,bb_graphics_LoadImage(String(L"Graphics/UI/Exit.png",20),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_starFull,bb_graphics_LoadImage(String(L"Graphics/UI/Star_Full.png",25),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_starFullLarge,bb_graphics_LoadImage(String(L"Graphics/UI/Star_Full_Large.png",31),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_levelH,bb_graphics_LoadImage(String(L"Graphics/UI/Level_Highlighted.png",33),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_starFullH,bb_graphics_LoadImage(String(L"Graphics/UI/Star_Full_Highlighted.png",37),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_rightArrowH,bb_graphics_LoadImage(String(L"Graphics/UI/Right_Arrow_Highlighted.png",39),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_leftArrowH,bb_graphics_LoadImage(String(L"Graphics/UI/Left_Arrow_Highlighted.png",38),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_menuH1,bb_graphics_LoadImage(String(L"Graphics/UI/Menu_Highlighted.png",32),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_optionsH1,bb_graphics_LoadImage(String(L"Graphics/UI/Options_Highlighted.png",35),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_playH,bb_graphics_LoadImage(String(L"Graphics/UI/Play_Highlighted.png",32),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_tutorialH,bb_graphics_LoadImage(String(L"Graphics/UI/Tutorial_Highlighted.png",36),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_optionsH2,bb_graphics_LoadImage(String(L"Graphics/UI/Options_Highlighted2.png",36),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_menuH2,bb_graphics_LoadImage(String(L"Graphics/UI/Menu_Highlighted2.png",33),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_menuH3,bb_graphics_LoadImage(String(L"Graphics/UI/Menu_Highlighted3.png",33),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_nextH,bb_graphics_LoadImage(String(L"Graphics/UI/Next_Highlighted.png",32),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_twitterH,bb_graphics_LoadImage(String(L"Graphics/UI/Twitter_Highlighted.png",35),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_redoH,bb_graphics_LoadImage(String(L"Graphics/UI/Redo_Highlighted.png",32),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_barrierV,bb_graphics_LoadImage(String(L"Graphics/Objects/Barrier_V.png",30),1,1));
	gc_assign(m_img_barrierH,bb_graphics_LoadImage(String(L"Graphics/Objects/Barrier_H.png",30),1,1));
	gc_assign(m_img_cross1,bb_graphics_LoadImage(String(L"Graphics/Objects/Cross1.png",27),1,1));
	gc_assign(m_img_cube1,bb_graphics_LoadImage(String(L"Graphics/Objects/Cube1.png",26),1,1));
	gc_assign(m_img_options,bb_graphics_LoadImage(String(L"Graphics/UI/Options.png",23),1,1));
	gc_assign(m_img_res1H,bb_graphics_LoadImage(String(L"Graphics/UI/Res1H.png",21),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_res2H,bb_graphics_LoadImage(String(L"Graphics/UI/Res2H.png",21),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_res3H,bb_graphics_LoadImage(String(L"Graphics/UI/Res3H.png",21),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_windowedH,bb_graphics_LoadImage(String(L"Graphics/UI/WindowedH.png",25),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_fullscreenH,bb_graphics_LoadImage(String(L"Graphics/UI/FullscreenH.png",27),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_creditsH,bb_graphics_LoadImage(String(L"Graphics/UI/CreditsH.png",24),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_exitGameH,bb_graphics_LoadImage(String(L"Graphics/UI/Exit_GameH.png",26),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_returnH,bb_graphics_LoadImage(String(L"Graphics/UI/ReturnH.png",23),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_stage2,bb_graphics_LoadImage(String(L"Graphics/UI/Stage2.png",22),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_stage3,bb_graphics_LoadImage(String(L"Graphics/UI/Stage3.png",22),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_completed,bb_graphics_LoadImage(String(L"Graphics/UI/Complete.png",24),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_androidOptions,bb_graphics_LoadImage(String(L"Graphics/UI/Options_Android_Html.png",36),1,1));
	gc_assign(m_img_on,bb_graphics_LoadImage(String(L"Graphics/UI/On.png",18),1,c_Image::m_DefaultFlags));
	gc_assign(m_img_off,bb_graphics_LoadImage(String(L"Graphics/UI/Off.png",19),1,c_Image::m_DefaultFlags));
	gc_assign(m_fnt_stageFont,(new c_BitmapFont)->m_new(String(L"Graphics/Fonts/Stage_Font.txt",29),false));
	gc_assign(m_fnt_stageSelectFont,(new c_BitmapFont)->m_new(String(L"Graphics/Fonts/Stage_Select_Font.txt",36),false));
	gc_assign(m_fnt_stageSelectFontH,(new c_BitmapFont)->m_new(String(L"Graphics/Fonts/Stage_Select_FontH.txt",37),false));
	gc_assign(m_fnt_72Font,(new c_BitmapFont)->m_new(String(L"Graphics/Fonts/72Font.txt",25),false));
	gc_assign(m_fnt_54Font,(new c_BitmapFont)->m_new(String(L"Graphics/Fonts/54Font.txt",25),false));
	gc_assign(m_fnt_timeFont,(new c_BitmapFont)->m_new(String(L"Graphics/Fonts/Time_Font.txt",28),false));
	m_debug=false;
	gc_assign(m_world,(new c_Box2D_World)->m_new(FLOAT(0.0),FLOAT(10.0),FLOAT(64.0),m_debug));
	m_tutorial=false;
	m_tutStep=0;
	m_options=false;
	m_fullscreen=false;
	m_resX=1280;
	m_resY=720;
	m_credits=false;
	m_creditStep=1;
	m_level=1;
	m_stage=1;
	m_hasWon=false;
	m_levelStartTimer=0;
	m_finalTime=FLOAT(0.0);
	gc_assign(m_barrierList,(new c_List2)->m_new());
	m_touchDelayStart=0;
	m_touchDelayTime=90;
	m_main_menu=true;
	m_isPlaying=false;
	m_levelComplete=false;
	m_oldSelector=9999;
	m_selector=9999;
	m_mX=bb_autofit_VTouchX(0,true);
	m_mY=bb_autofit_VTouchY(0,true);
	m_gamepad=false;
	m_gameProgress=0;
	gc_assign(m_gameData,(new c_GameData)->m_new());
	String t_saveData=bb_app_LoadState();
	int t_mus=1;
	if(t_saveData==String()){
		t_mus=1;
		bb_app_SaveState(m_gameData->p_SaveString(m_gameProgress,t_mus));
	}else{
		Array<String > t_s3=t_saveData.Split(String(L",",1));
		m_gameData->p_LoadString(t_saveData);
		bb_Rebound_versionCode=(t_s3[t_s3.Length()-3]).ToInt();
		if(bb_Rebound_versionCode==bb_Rebound_currentVersionCode){
			Array<String > t_s=t_saveData.Split(String(L",",1));
			m_gameProgress=(t_s[t_s.Length()-2]).ToInt();
			t_mus=(t_s[t_s.Length()-1]).ToInt();
			bbPrint(String(L"LOADED GAME DATA",16));
		}else{
			if(bb_Rebound_versionCode<105){
				Array<String > t_s2=t_saveData.Split(String(L",",1));
				m_gameProgress=(t_s2[t_s2.Length()-1]).ToInt();
				bb_app_SaveState(m_gameData->p_SaveString(m_gameProgress,1));
				t_saveData=String();
				t_saveData=bb_app_LoadState();
				gc_assign(m_gameData,(new c_GameData)->m_new());
				m_gameData->p_LoadString(t_saveData);
				Array<String > t_s5=t_saveData.Split(String(L",",1));
				m_gameProgress=(t_s5[t_s5.Length()-2]).ToInt();
				t_mus=(t_s5[t_s5.Length()-1]).ToInt();
				bbPrint(String(L"LOADED NEW VERSION GAME DATA",28));
			}else{
				Array<String > t_s4=t_saveData.Split(String(L",",1));
				bbPrint(String(L"length: ",8)+String(t_s4.Length()));
				m_gameProgress=(t_s4[t_s4.Length()-1]).ToInt();
				bbPrint(String(L"gameProgress: ",14)+String(m_gameProgress));
				bb_app_SaveState(m_gameData->p_SaveString(m_gameProgress,t_mus));
				t_saveData=String();
				t_saveData=bb_app_LoadState();
				gc_assign(m_gameData,(new c_GameData)->m_new());
				m_gameData->p_LoadString(t_saveData);
				Array<String > t_s52=t_saveData.Split(String(L",",1));
				m_gameProgress=(t_s52[t_s52.Length()-2]).ToInt();
				t_mus=(t_s52[t_s52.Length()-1]).ToInt();
				bbPrint(String(L"LOADED NEW VERSION GAME DATA",28));
			}
		}
	}
	m_gameData->m_stage[m_stage-1]->m_level[m_level-1]->m_unlocked=true;
	if(t_mus==1){
		m_musicPlaying=true;
	}else{
		m_musicPlaying=false;
	}
	p_BuildLevel(m_level);
	m_playerFriction=500000000000;
	gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(400.0),FLOAT(476.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),false));
	m_maxAngularVelocity=FLOAT(7.5);
	bb_app_SetUpdateRate(0);
	bb_app_ShowMouse();
	bb_audio_PlayMusic(String(L"Music/Ouroboros.ogg",19),1);
	if(m_musicPlaying==false){
		bb_audio_PauseMusic();
	}
	return 0;
}
void c_Game::p_ResetBarriers(int t_l){
	c_Enumerator3* t_=m_barrierList->p_ObjectEnumerator();
	while(t_->p_HasNext()){
		c_Barrier* t_b=t_->p_NextObject();
		t_b->m_ent->p_Kill();
	}
	m_barrierList->p_Clear();
	if(m_stage==1){
		if(t_l==5){
			m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_cross1,FLOAT(960.0),FLOAT(350.0),FLOAT(600.0),FLOAT(135.0),FLOAT(600.0),FLOAT(946.0),4,FLOAT(0.03),true));
		}else{
			if(t_l==6){
				m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_barrierH,FLOAT(234.0),FLOAT(1000.0),FLOAT(134.0),FLOAT(1000.0),FLOAT(600.0),FLOAT(1000.0),2,FLOAT(3.0),true));
				m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_barrierH,FLOAT(1686.0),FLOAT(1000.0),FLOAT(1320.0),FLOAT(1000.0),FLOAT(1786.0),FLOAT(1000.0),3,FLOAT(3.0),true));
			}else{
				if(t_l==7){
					Float t_x=FLOAT(475.0);
					Float t_y=FLOAT(622.0);
					for(int t_i=0;t_i<=6;t_i=t_i+1){
						m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_cube1,t_x,t_y,FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),6,FLOAT(3.0),true));
						t_y=t_y-FLOAT(33.0);
					}
					t_x=FLOAT(1180.0);
					t_y=FLOAT(1044.0);
					for(int t_i2=0;t_i2<=5;t_i2=t_i2+1){
						m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_cube1,t_x,t_y,FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),6,FLOAT(3.0),true));
						t_y=t_y-FLOAT(32.0);
					}
					m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_cross1,FLOAT(770.0),FLOAT(860.0),FLOAT(600.0),FLOAT(135.0),FLOAT(600.0),FLOAT(946.0),4,FLOAT(0.03),true));
				}else{
					if(t_l==8){
						m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_barrierV,FLOAT(710.0),FLOAT(472.0),FLOAT(600.0),FLOAT(135.0),FLOAT(600.0),FLOAT(946.0),0,FLOAT(3.0),true));
						m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_barrierV,FLOAT(1210.0),FLOAT(608.0),FLOAT(600.0),FLOAT(135.0),FLOAT(600.0),FLOAT(946.0),1,FLOAT(3.0),true));
					}
				}
			}
		}
	}else{
		if(m_stage==2){
			if(t_l==1){
				Float t_x2=FLOAT(725.0);
				Float t_y2=FLOAT(641.0);
				for(int t_i3=0;t_i3<=6;t_i3=t_i3+1){
					m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_cube1,t_x2,t_y2,FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),6,FLOAT(3.0),true));
					t_y2=t_y2-FLOAT(33.0);
				}
				t_x2=FLOAT(1213.0);
				t_y2=FLOAT(642.0);
				for(int t_i4=0;t_i4<=6;t_i4=t_i4+1){
					m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_cube1,t_x2,t_y2,FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),6,FLOAT(3.0),true));
					t_y2=t_y2-FLOAT(32.0);
				}
			}else{
				if(t_l==2){
					m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_barrierV,FLOAT(780.0),FLOAT(135.0),FLOAT(600.0),FLOAT(135.0),FLOAT(600.0),FLOAT(946.0),0,FLOAT(3.0),true));
					m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_barrierV,FLOAT(1336.0),FLOAT(595.0),FLOAT(600.0),FLOAT(135.0),FLOAT(600.0),FLOAT(946.0),0,FLOAT(3.0),true));
				}else{
					if(t_l==3){
					}else{
						if(t_l==4){
							Float t_x3=FLOAT(200.0);
							Float t_y3=FLOAT(240.0);
							do{
								m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_cube1,t_x3,t_y3,FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),6,FLOAT(3.0),true));
								t_x3=t_x3+FLOAT(100.0);
								if(t_x3>FLOAT(1700.0)){
									t_y3=t_y3+FLOAT(100.0);
									t_x3=FLOAT(200.0);
								}
							}while(!(t_y3>FLOAT(900.0)));
						}else{
							if(t_l==5){
							}else{
								if(t_l==6){
									m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_barrierV,FLOAT(400.0),FLOAT(115.5),FLOAT(600.0),FLOAT(115.5),FLOAT(600.0),FLOAT(964.5),0,FLOAT(3.2),true));
									m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_barrierV,FLOAT(1580.0),FLOAT(145.0),FLOAT(600.0),FLOAT(115.5),FLOAT(600.0),FLOAT(964.5),1,FLOAT(3.2),true));
								}else{
									if(t_l==7){
										m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_barrierV,FLOAT(700.0),FLOAT(360.0),FLOAT(600.0),FLOAT(360.0),FLOAT(600.0),FLOAT(760.0),0,FLOAT(3.2),true));
										m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_barrierV,FLOAT(1220.0),FLOAT(760.0),FLOAT(600.0),FLOAT(360.0),FLOAT(600.0),FLOAT(760.0),1,FLOAT(3.2),true));
										m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_barrierH,FLOAT(815.5),FLOAT(231.0),FLOAT(815.5),FLOAT(144.5),FLOAT(1104.5),FLOAT(144.5),2,FLOAT(2.32),true));
										m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_barrierH,FLOAT(1104.5),FLOAT(889.0),FLOAT(815.5),FLOAT(975.5),FLOAT(1104.5),FLOAT(975.5),3,FLOAT(2.32),true));
									}else{
										if(t_l==8){
											m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_cross1,FLOAT(960.0),FLOAT(540.0),FLOAT(600.0),FLOAT(135.0),FLOAT(600.0),FLOAT(946.0),4,FLOAT(0.03),true));
											Float t_x4=FLOAT(720.0);
											Float t_y4=FLOAT(235.0);
											for(int t_i5=0;t_i5<=6;t_i5=t_i5+1){
												m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_cube1,t_x4,t_y4,FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),6,FLOAT(3.0),true));
												t_y4=t_y4-FLOAT(32.0);
											}
											t_x4=FLOAT(1200.0);
											t_y4=FLOAT(235.0);
											for(int t_i6=0;t_i6<=6;t_i6=t_i6+1){
												m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_cube1,t_x4,t_y4,FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),6,FLOAT(3.0),true));
												t_y4=t_y4-FLOAT(32.0);
											}
											t_x4=FLOAT(1200.0);
											t_y4=FLOAT(1037.0);
											for(int t_i7=0;t_i7<=6;t_i7=t_i7+1){
												m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_cube1,t_x4,t_y4,FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),6,FLOAT(3.0),true));
												t_y4=t_y4-FLOAT(32.0);
											}
											t_x4=FLOAT(720.0);
											t_y4=FLOAT(1037.0);
											for(int t_i8=0;t_i8<=6;t_i8=t_i8+1){
												m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_cube1,t_x4,t_y4,FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),6,FLOAT(3.0),true));
												t_y4=t_y4-FLOAT(32.0);
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}else{
			if(m_stage==3){
				if(t_l==1){
					m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_barrierH,FLOAT(475.5),FLOAT(1020.0),FLOAT(475.5),FLOAT(1558.5),FLOAT(1443.5),FLOAT(1558.5),2,FLOAT(2.32),true));
				}else{
					if(t_l==2){
						m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_cross1,FLOAT(400.0),FLOAT(580.0),FLOAT(600.0),FLOAT(135.0),FLOAT(600.0),FLOAT(946.0),4,FLOAT(0.03),true));
					}else{
						if(t_l==3){
						}else{
							if(t_l==4){
							}else{
								if(t_l==5){
									m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_cross1,FLOAT(675.0),FLOAT(900.0),FLOAT(600.0),FLOAT(135.0),FLOAT(600.0),FLOAT(946.0),4,FLOAT(0.03),true));
									m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_cross1,FLOAT(1260.0),FLOAT(900.0),FLOAT(600.0),FLOAT(135.0),FLOAT(600.0),FLOAT(946.0),4,FLOAT(0.03),true));
								}else{
									if(t_l==6){
									}else{
										if(t_l==7){
										}else{
											if(t_l==8){
												m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_barrierV,FLOAT(710.0),FLOAT(250.0),FLOAT(600.0),FLOAT(135.0),FLOAT(600.0),FLOAT(946.0),1,FLOAT(3.0),true));
												m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_barrierV,FLOAT(1210.0),FLOAT(830.0),FLOAT(600.0),FLOAT(135.0),FLOAT(600.0),FLOAT(946.0),0,FLOAT(3.0),true));
												Float t_x5=FLOAT(475.0);
												Float t_y5=FLOAT(1044.0);
												for(int t_i9=0;t_i9<=6;t_i9=t_i9+1){
													m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_cube1,t_x5,t_y5,FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),6,FLOAT(3.0),true));
													t_y5=t_y5-FLOAT(32.0);
												}
												t_x5=FLOAT(960.0);
												t_y5=FLOAT(1044.0);
												for(int t_i10=0;t_i10<=5;t_i10=t_i10+1){
													m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_cube1,t_x5,t_y5,FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),6,FLOAT(3.0),true));
													t_y5=t_y5-FLOAT(32.0);
												}
												t_x5=FLOAT(1445.0);
												t_y5=FLOAT(1044.0);
												for(int t_i11=0;t_i11<=5;t_i11=t_i11+1){
													m_barrierList->p_AddLast2((new c_Barrier)->m_new(m_world,m_img_cube1,t_x5,t_y5,FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),6,FLOAT(3.0),true));
													t_y5=t_y5-FLOAT(32.0);
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}
void c_Game::p_CreateExit(Float t_x,Float t_y){
	c_b2Body* t_sensor=0;
	c_b2PolygonShape* t_box=(new c_b2PolygonShape)->m_new();
	t_box->p_SetAsBox(FLOAT(0.78125),FLOAT(0.390625));
	c_b2FixtureDef* t_fd=(new c_b2FixtureDef)->m_new();
	gc_assign(t_fd->m_shape,(t_box));
	t_fd->m_density=FLOAT(4.0);
	t_fd->m_friction=FLOAT(0.4);
	t_fd->m_restitution=FLOAT(0.3);
	gc_assign(t_fd->m_userData,((new c_StringObject)->m_new3(String(L"sensor",6))));
	t_fd->m_isSensor=true;
	c_b2BodyDef* t_bd=(new c_b2BodyDef)->m_new();
	t_bd->m_type=0;
	t_bd->m_position->p_Set2((t_x+FLOAT(50.0))/FLOAT(64.0),(t_y+FLOAT(25.0))/FLOAT(64.0));
	t_sensor=m_world->m_world->p_CreateBody(t_bd);
	t_sensor->p_CreateFixture(t_fd);
	m_world->m_world->p_SetContactListener((new c_SensorContactListener)->m_new(t_sensor,m_player->m_body));
	m_exitX=t_x;
	m_exitY=t_y;
}
void c_Game::p_CreateExit2(int t_l){
	if(m_stage==0){
		if(t_l==1){
			p_CreateExit(FLOAT(1700.0),FLOAT(957.0));
		}else{
			if(t_l==2){
				p_CreateExit(FLOAT(700.0),FLOAT(957.0));
			}
		}
	}else{
		if(m_stage==1){
			if(t_l==1){
				p_CreateExit(FLOAT(1700.0),FLOAT(957.0));
			}else{
				if(t_l==2){
					p_CreateExit(FLOAT(1700.0),FLOAT(957.0));
				}else{
					if(t_l==3){
						p_CreateExit(FLOAT(1700.0),FLOAT(840.0));
					}else{
						if(t_l==4){
							p_CreateExit(FLOAT(1700.0),FLOAT(957.0));
						}else{
							if(t_l==5){
								p_CreateExit(FLOAT(1700.0),FLOAT(600.0));
							}else{
								if(t_l==6){
									p_CreateExit(FLOAT(910.0),FLOAT(800.0));
								}else{
									if(t_l==7){
										p_CreateExit(FLOAT(1700.0),FLOAT(957.0));
									}else{
										if(t_l==8){
											p_CreateExit(FLOAT(1700.0),FLOAT(840.0));
										}
									}
								}
							}
						}
					}
				}
			}
		}else{
			if(m_stage==2){
				if(t_l==1){
					p_CreateExit(FLOAT(1575.0),FLOAT(957.0));
				}else{
					if(t_l==2){
						p_CreateExit(FLOAT(1700.0),FLOAT(515.0));
					}else{
						if(t_l==3){
							p_CreateExit(FLOAT(715.0),FLOAT(535.0));
						}else{
							if(t_l==4){
								p_CreateExit(FLOAT(535.0),FLOAT(957.0));
							}else{
								if(t_l==5){
									p_CreateExit(FLOAT(910.0),FLOAT(535.0));
								}else{
									if(t_l==6){
										p_CreateExit(FLOAT(910.0),FLOAT(900.0));
									}else{
										if(t_l==7){
											p_CreateExit(FLOAT(910.0),FLOAT(515.0));
										}else{
											if(t_l==8){
												p_CreateExit(FLOAT(1700.0),FLOAT(515.0));
											}
										}
									}
								}
							}
						}
					}
				}
			}else{
				if(m_stage==3){
					if(t_l==1){
						p_CreateExit(FLOAT(1700.0),FLOAT(840.0));
					}else{
						if(t_l==2){
							p_CreateExit(FLOAT(1700.0),FLOAT(957.0));
						}else{
							if(t_l==3){
								p_CreateExit(FLOAT(480.0),FLOAT(800.0));
							}else{
								if(t_l==4){
									p_CreateExit(FLOAT(1494.0),FLOAT(988.0));
								}else{
									if(t_l==5){
										p_CreateExit(FLOAT(1700.0),FLOAT(840.0));
									}else{
										if(t_l==6){
											p_CreateExit(FLOAT(1565.0),FLOAT(669.0));
										}else{
											if(t_l==7){
												p_CreateExit(FLOAT(1700.0),FLOAT(777.0));
											}else{
												if(t_l==8){
													p_CreateExit(FLOAT(1700.0),FLOAT(957.0));
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}
void c_Game::p_ResetPlayer(int t_l){
	m_player->p_Kill();
	if(m_stage==0){
		if(t_l==1){
			gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(960.0),FLOAT(540.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),true));
		}else{
			if(t_l==2){
				gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(400.0),FLOAT(593.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),true));
			}
		}
	}else{
		if(m_stage==1){
			if(t_l==1){
				gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(400.0),FLOAT(593.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),false));
			}else{
				if(t_l==2){
					gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(320.0),FLOAT(593.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),false));
				}else{
					if(t_l==3){
						gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(180.0),FLOAT(476.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),false));
					}else{
						if(t_l==4){
							gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(320.0),FLOAT(593.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),false));
						}else{
							if(t_l==5){
								gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(400.0),FLOAT(200.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),false));
							}else{
								if(t_l==6){
									gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(960.0),FLOAT(140.0),false,FLOAT(0.83),Float(m_playerFriction),FLOAT(5000.0),false));
									m_player->m_body->p_SetLinearVelocity((new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(1.2)));
								}else{
									if(t_l==7){
										gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(260.0),FLOAT(200.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),false));
									}else{
										if(t_l==8){
											gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(400.0),FLOAT(520.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),false));
										}
									}
								}
							}
						}
					}
				}
			}
		}else{
			if(m_stage==2){
				if(t_l==1){
					gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(320.0),FLOAT(530.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),false));
					m_player->m_body->p_SetLinearVelocity((new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(5.0)));
				}else{
					if(t_l==2){
						gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(60.0),FLOAT(540.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),false));
						m_player->m_body->p_SetLinearVelocity((new c_b2Vec2)->m_new(FLOAT(2.0),FLOAT(0.0)));
					}else{
						if(t_l==3){
							gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(1400.0),FLOAT(560.0),false,FLOAT(.89),Float(m_playerFriction),FLOAT(5000.0),false));
							m_player->m_body->p_SetAngle(FLOAT(1.575));
							m_player->m_body->p_SetLinearVelocity((new c_b2Vec2)->m_new(FLOAT(4.0),FLOAT(0.0)));
						}else{
							if(t_l==4){
								gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(1700.0),FLOAT(590.0),false,FLOAT(.89),Float(m_playerFriction),FLOAT(5000.0),false));
								m_player->m_body->p_SetAngle(FLOAT(1.575));
								m_player->m_body->p_SetLinearVelocity((new c_b2Vec2)->m_new(FLOAT(-10.0),FLOAT(0.0)));
							}else{
								if(t_l==5){
									gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(1225.0),FLOAT(700.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),false));
									m_player->m_body->p_SetAngle(FLOAT(-0.785));
									m_player->m_body->p_SetLinearVelocity((new c_b2Vec2)->m_new(FLOAT(3.0),FLOAT(3.0)));
								}else{
									if(t_l==6){
										gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(580.0),FLOAT(140.0),false,FLOAT(0.83),Float(m_playerFriction),FLOAT(5000.0),false));
										m_player->m_body->p_SetAngle(FLOAT(-1.285));
										m_player->m_body->p_SetLinearVelocity((new c_b2Vec2)->m_new(FLOAT(6.0),FLOAT(2.0)));
									}else{
										if(t_l==7){
											gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(400.0),FLOAT(540.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),false));
											m_player->m_body->p_SetAngle(FLOAT(1.575));
											m_player->m_body->p_SetLinearVelocity((new c_b2Vec2)->m_new(FLOAT(-3.5),FLOAT(0.0)));
										}else{
											if(t_l==8){
												gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(400.0),FLOAT(540.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),false));
												m_player->m_body->p_SetAngle(FLOAT(1.575));
												m_player->m_body->p_SetLinearVelocity((new c_b2Vec2)->m_new(FLOAT(-4.5),FLOAT(0.0)));
											}
										}
									}
								}
							}
						}
					}
				}
			}else{
				if(m_stage==3){
					if(t_l==1){
						gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(250.0),FLOAT(520.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),false));
					}else{
						if(t_l==2){
							gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(1500.0),FLOAT(140.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),false));
						}else{
							if(t_l==3){
								gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(500.0),FLOAT(-114.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),false));
							}else{
								if(t_l==4){
									gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(400.0),FLOAT(200.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),false));
								}else{
									if(t_l==5){
										gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(400.0),FLOAT(520.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),false));
									}else{
										if(t_l==6){
											gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(300.0),FLOAT(396.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),false));
										}else{
											if(t_l==7){
												gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(400.0),FLOAT(593.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),false));
											}else{
												if(t_l==8){
													gc_assign(m_player,this->m_world->p_CreateImageBox(this->m_img_player,FLOAT(300.0),FLOAT(593.0),false,FLOAT(0.89),Float(m_playerFriction),FLOAT(5000.0),false));
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	m_levelStartTimer=bb_app_Millisecs();
	p_ResetBarriers(t_l);
	p_CreateExit2(t_l);
}
void c_Game::p_LoadLevel2(int t_l){
	m_world->p_Clear();
	gc_assign(m_world,(new c_Box2D_World)->m_new(FLOAT(0.0),FLOAT(10.0),FLOAT(64.0),m_debug));
	p_BuildLevel(t_l);
	p_ResetPlayer(t_l);
	m_isPlaying=true;
	m_levelStartTimer=bb_app_Millisecs();
	m_selector=9999;
	m_touchDelayStart=bb_app_Millisecs();
}
void c_Game::p_LoadLevelBackground(int t_l){
	m_img_level->p_Discard();
	if(m_stage==1){
		if(t_l==1){
			gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level1.png",26),1,c_Image::m_DefaultFlags));
		}else{
			if(t_l==2){
				gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level2.png",26),1,c_Image::m_DefaultFlags));
			}else{
				if(t_l==3){
					gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level3.png",26),1,c_Image::m_DefaultFlags));
				}else{
					if(t_l==4){
						gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level4.png",26),1,c_Image::m_DefaultFlags));
					}else{
						if(t_l==5){
							gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level5.png",26),1,c_Image::m_DefaultFlags));
						}else{
							if(t_l==6){
								gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level6.png",26),1,c_Image::m_DefaultFlags));
							}else{
								if(t_l==7){
									gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level7.png",26),1,c_Image::m_DefaultFlags));
								}else{
									if(t_l==8){
										gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level8.png",26),1,c_Image::m_DefaultFlags));
									}
								}
							}
						}
					}
				}
			}
		}
	}else{
		if(m_stage==2){
			if(t_l==1){
				gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level9.png",26),1,c_Image::m_DefaultFlags));
			}else{
				if(t_l==2){
					gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level10.png",27),1,c_Image::m_DefaultFlags));
				}else{
					if(t_l==3){
						gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level11.png",27),1,c_Image::m_DefaultFlags));
					}else{
						if(t_l==4){
							gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level1.png",26),1,c_Image::m_DefaultFlags));
						}else{
							if(t_l==5){
								gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level13.png",27),1,c_Image::m_DefaultFlags));
							}else{
								if(t_l==6){
									gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level14.png",27),1,c_Image::m_DefaultFlags));
								}else{
									if(t_l==7){
										gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level1.png",26),1,c_Image::m_DefaultFlags));
									}else{
										if(t_l==8){
											gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level1.png",26),1,c_Image::m_DefaultFlags));
										}
									}
								}
							}
						}
					}
				}
			}
		}else{
			if(m_stage==3){
				if(t_l==1){
					gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level17.png",27),1,c_Image::m_DefaultFlags));
				}else{
					if(t_l==2){
						gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level18.png",27),1,c_Image::m_DefaultFlags));
					}else{
						if(t_l==3){
							gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level19.png",27),1,c_Image::m_DefaultFlags));
						}else{
							if(t_l==4){
								gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level20.png",27),1,c_Image::m_DefaultFlags));
							}else{
								if(t_l==5){
									gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level8.png",26),1,c_Image::m_DefaultFlags));
								}else{
									if(t_l==6){
										gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level22.png",27),1,c_Image::m_DefaultFlags));
									}else{
										if(t_l==7){
											gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level23.png",27),1,c_Image::m_DefaultFlags));
										}else{
											if(t_l==8){
												gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level1.png",26),1,c_Image::m_DefaultFlags));
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}
void c_Game::p_LoadLevel(int t_l){
	m_world->p_Clear();
	gc_assign(m_world,(new c_Box2D_World)->m_new(FLOAT(0.0),FLOAT(10.0),FLOAT(64.0),m_debug));
	p_LoadLevelBackground(t_l);
	p_BuildLevel(t_l);
	p_ResetPlayer(t_l);
	m_isPlaying=true;
	m_levelStartTimer=bb_app_Millisecs();
	m_selector=9999;
	m_touchDelayStart=bb_app_Millisecs();
}
bool c_Game::m_sensorColliding;
void c_Game::p_SetResolution(int t_width,int t_height,bool t_fullscreen){
	bb_app_SetDeviceWindow(t_width,t_height,((t_fullscreen)?1:0));
	if(m_gamepad==false){
		bb_app_ShowMouse();
	}
}
int c_Game::p_OnUpdate(){
	if(m_isPlaying==true){
		if(m_tutorial==false){
			if(((bb_input_KeyDown(68))!=0) || ((bb_input_KeyDown(39))!=0)){
				m_player->m_body->p_SetAngularVelocity(m_player->m_body->p_GetAngularVelocity()+FLOAT(0.1));
				if(m_player->m_body->p_GetAngularVelocity()>m_maxAngularVelocity){
					m_player->m_body->p_SetAngularVelocity(m_maxAngularVelocity);
				}
			}
			if(((bb_input_KeyDown(65))!=0) || ((bb_input_KeyDown(37))!=0)){
				m_player->m_body->p_SetAngularVelocity(m_player->m_body->p_GetAngularVelocity()-FLOAT(0.1));
				if(m_player->m_body->p_GetAngularVelocity()<-m_maxAngularVelocity){
					m_player->m_body->p_SetAngularVelocity(-m_maxAngularVelocity);
				}
			}
			if(m_gamepad==true){
				if(bb_input_JoyZ(0,0)<FLOAT(-0.01) || ((bb_input_JoyDown(10,0))!=0)){
					m_player->m_body->p_SetAngularVelocity(m_player->m_body->p_GetAngularVelocity()+FLOAT(0.1));
					if(m_player->m_body->p_GetAngularVelocity()>m_maxAngularVelocity){
						m_player->m_body->p_SetAngularVelocity(m_maxAngularVelocity);
					}
				}
				if(bb_input_JoyZ(0,0)>FLOAT(0.01) || ((bb_input_JoyDown(8,0))!=0)){
					m_player->m_body->p_SetAngularVelocity(m_player->m_body->p_GetAngularVelocity()-FLOAT(0.1));
					if(m_player->m_body->p_GetAngularVelocity()<-m_maxAngularVelocity){
						m_player->m_body->p_SetAngularVelocity(-m_maxAngularVelocity);
					}
				}
				if((bb_input_JoyHit(2,0))!=0){
					p_ResetPlayer(m_level);
				}
				if((bb_input_JoyHit(1,0))!=0){
					m_isPlaying=false;
					m_main_menu=false;
					m_levelComplete=false;
					m_selector=(m_stage-1)*8+m_level;
				}
			}
			if(m_gamepad==false){
				if(bb_autofit_VTouchX(0,true)>=FLOAT(20.0) && bb_autofit_VTouchX(0,true)<=FLOAT(200.0)){
					if(bb_autofit_VTouchY(0,true)>=FLOAT(20.0) && bb_autofit_VTouchY(0,true)<=FLOAT(200.0)){
						if((bb_input_TouchHit(0))!=0){
							p_LoadLevel2(m_level);
							m_touchDelayStart=bb_app_Millisecs();
						}
						m_selector=1;
					}else{
						m_selector=9999;
					}
				}else{
					m_selector=9999;
				}
				if((bb_input_KeyHit(82))!=0){
					p_LoadLevel(m_level);
				}
				if(m_selector==9999){
					if(bb_autofit_VTouchX(0,true)>=FLOAT(1715.0) && bb_autofit_VTouchX(0,true)<=FLOAT(1885.0)){
						if(bb_autofit_VTouchY(0,true)>=FLOAT(25.0) && bb_autofit_VTouchY(0,true)<=FLOAT(80.0)){
							if((bb_input_TouchHit(0))!=0){
								m_levelComplete=false;
								m_isPlaying=false;
							}
							m_selector=2;
						}else{
							m_selector=9999;
						}
					}else{
						m_selector=9999;
					}
				}
			}
			if(m_sensorColliding){
				m_levelComplete=true;
				m_finalTime=Float(bb_app_Millisecs()-m_levelStartTimer);
				int t_tt=int(m_finalTime);
				String t_st=String(t_tt/1000)+String(L".",1);
				String t_mm=String(t_tt);
				int t_len=t_mm.Length();
				t_st=t_st+t_mm.Slice(t_len-3,t_len-2);
				m_fTime=t_st;
				m_tStars=bb_Rebound_AssignStars(m_stage,m_level,m_finalTime);
				m_gameData->p_CompleteLevel(m_stage,m_level,m_finalTime);
				int t_mus=0;
				if(m_musicPlaying==true){
					t_mus=1;
				}
				bb_app_SaveState(m_gameData->p_SaveString(m_gameProgress,t_mus));
				m_isPlaying=false;
				m_selector=1;
			}
			c_Enumerator3* t_=m_barrierList->p_ObjectEnumerator();
			while(t_->p_HasNext()){
				c_Barrier* t_b=t_->p_NextObject();
				t_b->p_Update();
			}
		}else{
			if(m_tutorial==true){
				if(m_tutStep==0){
					if(bb_app_Millisecs()>=m_tutTime+m_tutWait){
						m_tutStep=1;
						m_tutTime=bb_app_Millisecs();
					}
				}else{
					if(m_tutStep==1){
						if(bb_app_Millisecs()>=m_tutTime+m_tutWait){
							m_tutStep=2;
							m_tutTime=bb_app_Millisecs();
						}
					}else{
						if(m_tutStep==2){
							if(bb_app_Millisecs()>=m_tutTime+m_tutWait){
								m_tutStep=3;
								m_tutTime=bb_app_Millisecs();
							}
						}else{
							if(m_tutStep==3){
								if(bb_app_Millisecs()>=m_tutTime+m_tutWait){
									m_tutStep=4;
									m_tutTime=bb_app_Millisecs();
									p_LoadLevel(m_level);
									m_touchDelayStart=bb_app_Millisecs();
								}
							}else{
								if(m_tutStep==4){
									if(bb_input_JoyZ(0,0)<FLOAT(-0.01) || ((bb_input_JoyDown(10,0))!=0)){
										m_player->m_body->p_SetAngularVelocity(m_player->m_body->p_GetAngularVelocity()+FLOAT(0.1));
										if(m_player->m_body->p_GetAngularVelocity()>m_maxAngularVelocity){
											m_player->m_body->p_SetAngularVelocity(m_maxAngularVelocity);
											m_tutStep=5;
											m_tutTime=bb_app_Millisecs();
										}
									}
									if(((bb_input_KeyDown(68))!=0) || ((bb_input_KeyDown(39))!=0)){
										m_player->m_body->p_SetAngularVelocity(m_player->m_body->p_GetAngularVelocity()+FLOAT(0.1));
										if(m_player->m_body->p_GetAngularVelocity()>m_maxAngularVelocity){
											m_player->m_body->p_SetAngularVelocity(m_maxAngularVelocity);
											m_tutStep=5;
											m_tutTime=bb_app_Millisecs();
										}
									}
									m_player->m_body->p_SetPosition((new c_b2Vec2)->m_new(FLOAT(15.0),FLOAT(10.0)));
								}else{
									if(m_tutStep==5){
										if(bb_app_Millisecs()>=m_tutTime+m_tutWait){
											m_tutStep=6;
											m_img_level->p_Discard();
											gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/Levels/Level1.png",26),1,c_Image::m_DefaultFlags));
											m_tutTime=bb_app_Millisecs();
											p_ResetPlayer(m_level);
										}
										m_player->m_body->p_SetPosition((new c_b2Vec2)->m_new(FLOAT(15.0),FLOAT(10.0)));
									}else{
										if(m_tutStep==6){
											if(bb_input_JoyZ(0,0)>FLOAT(0.01) || ((bb_input_JoyDown(8,0))!=0)){
												m_player->m_body->p_SetAngularVelocity(m_player->m_body->p_GetAngularVelocity()-FLOAT(0.1));
												if(m_player->m_body->p_GetAngularVelocity()<-m_maxAngularVelocity){
													m_player->m_body->p_SetAngularVelocity(-m_maxAngularVelocity);
													m_tutStep=7;
													m_tutTime=bb_app_Millisecs();
												}
											}
											if(((bb_input_KeyDown(65))!=0) || ((bb_input_KeyDown(37))!=0)){
												m_player->m_body->p_SetAngularVelocity(m_player->m_body->p_GetAngularVelocity()-FLOAT(0.1));
												if(m_player->m_body->p_GetAngularVelocity()<-m_maxAngularVelocity){
													m_player->m_body->p_SetAngularVelocity(-m_maxAngularVelocity);
													m_tutStep=7;
													m_tutTime=bb_app_Millisecs();
												}
											}
											m_player->m_body->p_SetPosition((new c_b2Vec2)->m_new(FLOAT(15.0),FLOAT(10.0)));
										}else{
											if(m_tutStep==7){
												if(bb_app_Millisecs()>=m_tutTime+m_tutWait){
													m_tutStep=8;
													m_tutTime=bb_app_Millisecs();
													m_tutWait=3500;
													m_level=2;
													p_ResetPlayer(m_level);
													p_CreateExit(FLOAT(1700.0),FLOAT(946.0));
												}
												m_player->m_body->p_SetPosition((new c_b2Vec2)->m_new(FLOAT(15.0),FLOAT(10.0)));
											}else{
												if(m_tutStep==8){
													if(bb_app_Millisecs()>=m_tutTime+m_tutWait){
														m_tutStep=9;
														m_tutWait=3500;
														m_tutTime=bb_app_Millisecs();
													}
													m_player->m_body->p_SetPosition((new c_b2Vec2)->m_new(FLOAT(6.25),FLOAT(9.265625)));
												}else{
													if(m_tutStep==9){
														if(bb_app_Millisecs()>=m_tutTime+m_tutWait){
															m_tutStep=10;
															m_tutTime=bb_app_Millisecs();
															m_tutWait=2500;
														}
														m_player->m_body->p_SetPosition((new c_b2Vec2)->m_new(FLOAT(6.25),FLOAT(9.265625)));
													}else{
														if(m_tutStep==10){
															if(bb_app_Millisecs()>=m_tutTime+m_tutWait){
																m_tutStep=11;
																m_tutTime=bb_app_Millisecs();
																m_tutWait=4000;
																p_BuildLevel(m_level);
																p_ResetPlayer(m_level);
																p_CreateExit(FLOAT(1700.0),FLOAT(946.0));
															}
															m_player->m_body->p_SetPosition((new c_b2Vec2)->m_new(FLOAT(6.25),FLOAT(9.265625)));
														}else{
															if(m_tutStep==11){
																if(bb_app_Millisecs()>=m_tutTime+m_tutWait){
																	m_tutStep=12;
																	m_tutTime=bb_app_Millisecs();
																}
																if(bb_input_JoyZ(0,0)>FLOAT(0.01) || ((bb_input_JoyDown(8,0))!=0)){
																	m_player->m_body->p_SetAngularVelocity(m_player->m_body->p_GetAngularVelocity()-FLOAT(0.1));
																	if(m_player->m_body->p_GetAngularVelocity()<-m_maxAngularVelocity){
																		m_player->m_body->p_SetAngularVelocity(-m_maxAngularVelocity);
																	}
																}
																if(bb_input_JoyZ(0,0)<FLOAT(-0.01) || ((bb_input_JoyDown(10,0))!=0)){
																	m_player->m_body->p_SetAngularVelocity(m_player->m_body->p_GetAngularVelocity()+FLOAT(0.1));
																	if(m_player->m_body->p_GetAngularVelocity()>m_maxAngularVelocity){
																		m_player->m_body->p_SetAngularVelocity(m_maxAngularVelocity);
																	}
																}
																if(((bb_input_KeyDown(65))!=0) || ((bb_input_KeyDown(37))!=0)){
																	m_player->m_body->p_SetAngularVelocity(m_player->m_body->p_GetAngularVelocity()-FLOAT(0.1));
																	if(m_player->m_body->p_GetAngularVelocity()<-m_maxAngularVelocity){
																		m_player->m_body->p_SetAngularVelocity(-m_maxAngularVelocity);
																	}
																}
																if(((bb_input_KeyDown(68))!=0) || ((bb_input_KeyDown(39))!=0)){
																	m_player->m_body->p_SetAngularVelocity(m_player->m_body->p_GetAngularVelocity()+FLOAT(0.1));
																	if(m_player->m_body->p_GetAngularVelocity()>m_maxAngularVelocity){
																		m_player->m_body->p_SetAngularVelocity(m_maxAngularVelocity);
																	}
																}
															}else{
																if(m_tutStep==12){
																	if((bb_input_JoyHit(2,0))!=0){
																		p_ResetPlayer(m_level);
																		p_CreateExit(FLOAT(800.0),FLOAT(930.0));
																		m_tutStep=13;
																		m_tutWait=bb_app_Millisecs();
																	}
																	if(bb_autofit_VTouchX(0,true)>=FLOAT(20.0) && bb_autofit_VTouchX(0,true)<=FLOAT(200.0)){
																		if(bb_autofit_VTouchY(0,true)>=FLOAT(20.0) && bb_autofit_VTouchY(0,true)<=FLOAT(200.0)){
																			if((bb_input_TouchHit(0))!=0){
																				p_ResetPlayer(m_level);
																				m_tutStep=13;
																				m_tutWait=bb_app_Millisecs();
																				m_touchDelayStart=bb_app_Millisecs();
																			}
																			m_selector=1;
																		}else{
																			m_selector=9999;
																		}
																	}else{
																		m_selector=9999;
																	}
																	if((bb_input_KeyHit(82))!=0){
																		p_ResetPlayer(m_level);
																		p_CreateExit(FLOAT(800.0),FLOAT(930.0));
																		m_tutStep=13;
																		m_tutWait=bb_app_Millisecs();
																	}
																}else{
																	if(m_tutStep==13){
																		if(bb_input_JoyZ(0,0)>FLOAT(0.01) || ((bb_input_JoyDown(8,0))!=0)){
																			m_player->m_body->p_SetAngularVelocity(m_player->m_body->p_GetAngularVelocity()-FLOAT(0.1));
																			if(m_player->m_body->p_GetAngularVelocity()<-m_maxAngularVelocity){
																				m_player->m_body->p_SetAngularVelocity(-m_maxAngularVelocity);
																			}
																		}
																		if(bb_input_JoyZ(0,0)<FLOAT(-0.01) || ((bb_input_JoyDown(10,0))!=0)){
																			m_player->m_body->p_SetAngularVelocity(m_player->m_body->p_GetAngularVelocity()+FLOAT(0.1));
																			if(m_player->m_body->p_GetAngularVelocity()>m_maxAngularVelocity){
																				m_player->m_body->p_SetAngularVelocity(m_maxAngularVelocity);
																			}
																		}
																		if((bb_input_JoyHit(2,0))!=0){
																			p_ResetPlayer(m_level);
																		}
																		if(((bb_input_KeyDown(65))!=0) || ((bb_input_KeyDown(37))!=0)){
																			m_player->m_body->p_SetAngularVelocity(m_player->m_body->p_GetAngularVelocity()-FLOAT(0.1));
																			if(m_player->m_body->p_GetAngularVelocity()<-m_maxAngularVelocity){
																				m_player->m_body->p_SetAngularVelocity(-m_maxAngularVelocity);
																			}
																		}
																		if(((bb_input_KeyDown(68))!=0) || ((bb_input_KeyDown(39))!=0)){
																			m_player->m_body->p_SetAngularVelocity(m_player->m_body->p_GetAngularVelocity()+FLOAT(0.1));
																			if(m_player->m_body->p_GetAngularVelocity()>m_maxAngularVelocity){
																				m_player->m_body->p_SetAngularVelocity(m_maxAngularVelocity);
																			}
																		}
																		if(bb_autofit_VTouchX(0,true)>=FLOAT(20.0) && bb_autofit_VTouchX(0,true)<=FLOAT(200.0)){
																			if(bb_autofit_VTouchY(0,true)>=FLOAT(20.0) && bb_autofit_VTouchY(0,true)<=FLOAT(200.0)){
																				if((bb_input_TouchHit(0))!=0){
																					p_ResetPlayer(m_level);
																					m_touchDelayStart=bb_app_Millisecs();
																				}
																				m_selector=1;
																			}else{
																				m_selector=9999;
																			}
																		}else{
																			m_selector=9999;
																		}
																		if((bb_input_KeyHit(82))!=0){
																			p_ResetPlayer(m_level);
																		}
																		if(m_sensorColliding){
																			m_tutStep=14;
																			m_tutWait=2000;
																			m_tutTime=bb_app_Millisecs();
																			m_player->p_Kill();
																		}
																	}else{
																		if(m_tutStep==14){
																			if(bb_app_Millisecs()>=m_tutTime+m_tutWait){
																				m_tutStep=15;
																				m_tutTime=bb_app_Millisecs();
																				m_tutWait=4000;
																			}
																		}else{
																			if(m_tutStep==15){
																				if(bb_app_Millisecs()>=m_tutTime+m_tutWait){
																					m_tutStep=16;
																					m_tutTime=bb_app_Millisecs();
																					m_tutWait=2000;
																				}
																			}else{
																				if(m_tutStep==16){
																					if((bb_input_JoyHit(1,0))!=0){
																						m_tutStep=17;
																						m_tutTime=bb_app_Millisecs();
																						m_tutWait=3000;
																					}
																					if(bb_autofit_VTouchX(0,true)>=FLOAT(1715.0) && bb_autofit_VTouchX(0,true)<=FLOAT(1885.0)){
																						if(bb_autofit_VTouchY(0,true)>=FLOAT(25.0) && bb_autofit_VTouchY(0,true)<=FLOAT(80.0)){
																							if((bb_input_TouchHit(0))!=0){
																								m_tutStep=17;
																								m_tutTime=bb_app_Millisecs();
																								m_tutWait=3000;
																							}
																							m_selector=2;
																						}else{
																							m_selector=9999;
																						}
																					}else{
																						m_selector=9999;
																					}
																				}else{
																					if(m_tutStep==17){
																						if(bb_app_Millisecs()>=m_tutTime+m_tutWait){
																							m_tutStep=18;
																							m_tutTime=bb_app_Millisecs();
																							m_tutWait=2000;
																						}
																					}else{
																						if(m_tutStep==18){
																							if(bb_app_Millisecs()>=m_tutTime+m_tutWait){
																								m_tutorial=false;
																								m_tutStep=0;
																								m_isPlaying=false;
																								m_stage=1;
																								m_level=1;
																								m_main_menu=true;
																								m_world->p_Clear();
																							}
																						}
																					}
																				}
																			}
																		}
																	}
																}
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
		this->m_world->p_Update();
	}else{
		if(m_main_menu==true && m_options==false && m_credits==false){
			if(m_gamepad==true){
				if(m_selector==1){
					if((bb_input_JoyHit(10,0))!=0){
						m_selector=3;
					}else{
						if((bb_input_JoyHit(8,0))!=0){
							m_selector=2;
						}
					}
				}else{
					if(m_selector==2){
						if((bb_input_JoyHit(10,0))!=0){
							m_selector=1;
						}else{
							if((bb_input_JoyHit(8,0))!=0){
								m_selector=3;
							}
						}
					}else{
						if(m_selector==3){
							if((bb_input_JoyHit(10,0))!=0){
								m_selector=2;
							}else{
								if((bb_input_JoyHit(8,0))!=0){
									m_selector=1;
								}
							}
						}
					}
				}
				if((bb_input_JoyHit(0,0))!=0){
					if(m_selector==1){
						m_main_menu=false;
					}else{
						if(m_selector==2){
							m_tutorial=true;
							m_tutStep=0;
							m_isPlaying=true;
							m_stage=0;
							m_level=1;
							m_tutTime=bb_app_Millisecs();
							m_tutWait=2000;
							m_main_menu=false;
							m_world->p_Clear();
							m_selector=9999;
						}else{
							if(m_selector==3){
								m_options=true;
								m_selector=1;
							}
						}
					}
				}
			}
			if(m_gamepad==false){
				if(bb_autofit_VTouchX(0,true)>=FLOAT(700.0) && bb_autofit_VTouchX(0,true)<=FLOAT(1220.0)){
					if(bb_autofit_VTouchY(0,true)>=FLOAT(635.0) && bb_autofit_VTouchY(0,true)<=FLOAT(875.0)){
						if((bb_input_TouchHit(0))!=0){
							m_main_menu=false;
						}
						m_selector=1;
					}else{
						m_selector=9999;
					}
				}else{
					m_selector=9999;
				}
				if(m_selector==9999){
					if(bb_autofit_VTouchX(0,true)>=FLOAT(144.0) && bb_autofit_VTouchX(0,true)<=FLOAT(575.0)){
						if(bb_autofit_VTouchY(0,true)>=FLOAT(677.0) && bb_autofit_VTouchY(0,true)<=FLOAT(864.0)){
							if((bb_input_TouchHit(0))!=0){
								m_tutorial=true;
								m_tutStep=0;
								m_isPlaying=true;
								m_stage=0;
								m_level=1;
								m_tutTime=bb_app_Millisecs();
								m_tutWait=2000;
								m_main_menu=false;
								m_world->p_Clear();
							}
							m_selector=2;
						}else{
							m_selector=9999;
						}
					}else{
						m_selector=9999;
					}
				}
				if(m_selector==9999){
					if(bb_autofit_VTouchX(0,true)>=FLOAT(1343.0) && bb_autofit_VTouchX(0,true)<=FLOAT(1774.0)){
						if(bb_autofit_VTouchY(0,true)>=FLOAT(677.0) && bb_autofit_VTouchY(0,true)<=FLOAT(864.0)){
							if((bb_input_TouchHit(0))!=0){
								m_options=true;
								m_selector=1;
							}
							m_selector=3;
						}else{
							m_selector=9999;
						}
					}else{
						m_selector=9999;
					}
				}
			}
		}else{
			if(m_levelComplete==true && m_credits==false){
				if(m_gamepad==true){
					if(m_selector==1){
						if((bb_input_JoyHit(10,0))!=0){
							m_selector=2;
						}else{
							if((bb_input_JoyHit(8,0))!=0){
								m_selector=2;
							}else{
								if((bb_input_JoyHit(9,0))!=0){
									m_selector=3;
								}else{
									if((bb_input_JoyHit(11,0))!=0){
										m_selector=3;
									}
								}
							}
						}
					}else{
						if(m_selector==2){
							if((bb_input_JoyHit(10,0))!=0){
								m_selector=1;
							}else{
								if((bb_input_JoyHit(8,0))!=0){
									m_selector=1;
								}else{
									if((bb_input_JoyHit(9,0))!=0){
										m_selector=3;
									}else{
										if((bb_input_JoyHit(11,0))!=0){
											m_selector=3;
										}
									}
								}
							}
						}else{
							if(m_selector==3){
								if((bb_input_JoyHit(10,0))!=0){
									m_selector=2;
								}else{
									if((bb_input_JoyHit(8,0))!=0){
										m_selector=1;
									}else{
										if((bb_input_JoyHit(9,0))!=0){
											m_selector=1;
										}else{
											if((bb_input_JoyHit(11,0))!=0){
												m_selector=1;
											}
										}
									}
								}
							}
						}
					}
					if((bb_input_JoyHit(0,0))!=0){
						if(m_selector==1){
							m_level=m_level+1;
							if(m_level==9){
								if(m_stage==1){
									if(m_gameProgress==0){
										m_gameProgress=1;
										m_gameProgressTimer=bb_app_Millisecs();
										m_main_menu=false;
										m_levelComplete=false;
										m_selector=(m_stage-1)*8+m_level;
									}
								}else{
									if(m_stage==2){
										if(m_gameProgress==2){
											m_gameProgress=3;
											m_main_menu=false;
											m_levelComplete=false;
											m_gameProgressTimer=bb_app_Millisecs();
											m_selector=(m_stage-1)*8+m_level;
										}
									}else{
										if(m_stage==3){
											if(m_gameProgress==4){
												m_gameProgress=5;
												m_main_menu=false;
												m_levelComplete=false;
												m_gameProgressTimer=bb_app_Millisecs();
												m_selector=(m_stage-1)*8+24;
											}
										}
									}
								}
								if(m_stage<3){
									m_stage=m_stage+1;
									m_level=1;
									m_selector=(m_stage-1)*8+m_level;
								}else{
									m_stage=3;
									m_level=8;
									m_selector=(m_stage-1)*8+m_level;
									m_isPlaying=false;
								}
							}
							if(m_gameProgress!=1 && m_gameProgress!=3 && m_gameProgress!=5){
								m_world->p_Clear();
								p_BuildLevel(m_level);
								p_ResetPlayer(m_level);
								m_levelComplete=false;
								m_isPlaying=true;
								m_selector=0;
							}
						}else{
							if(m_selector==2){
								m_main_menu=false;
								m_levelComplete=false;
								m_selector=(m_stage-1)*8+m_level;
							}else{
								if(m_selector==3){
									String t_starText=String(L"Stars",5);
									if(m_tStars==1){
										t_starText=String(L"Star",4);
									}
									int t_levelNumber=0;
									if(m_stage==1){
										t_levelNumber=m_level;
									}else{
										t_levelNumber=m_level+8*(m_stage-1);
									}
									c_DoTweet::m_LaunchTwitter(String(),String(L"I completed Level ",18)+String(t_levelNumber)+String(L" and earned ",12)+String(m_tStars)+String(L" ",1)+t_starText+String(L"!",1),String(L"ReboundGame",11));
								}
							}
						}
					}
					if((bb_input_JoyHit(2,0))!=0){
						m_isPlaying=true;
						m_levelComplete=false;
						p_ResetPlayer(m_level);
						m_selector=1;
					}
					if((bb_input_JoyHit(1,0))!=0){
						m_main_menu=false;
						m_levelComplete=false;
						m_selector=(m_stage-1)*8+m_level;
					}
				}
				if(m_gamepad==false){
					if(bb_autofit_VTouchY(0,true)>=FLOAT(700.0) && bb_autofit_VTouchY(0,true)<=FLOAT(803.0)){
						if(bb_autofit_VTouchX(0,true)>=FLOAT(702.0) && bb_autofit_VTouchX(0,true)<=FLOAT(939.0)){
							if((bb_input_TouchHit(0))!=0){
								m_main_menu=false;
								m_levelComplete=false;
							}
							m_selector=2;
						}else{
							m_selector=9999;
						}
						if(m_selector==9999){
							if(bb_autofit_VTouchX(0,true)>=FLOAT(981.0) && bb_autofit_VTouchX(0,true)<=FLOAT(1254.0)){
								if((bb_input_TouchHit(0))!=0){
									m_level=m_level+1;
									if(m_level==9){
										if(m_stage==1){
											if(m_gameProgress==0){
												m_gameProgress=1;
												m_gameProgressTimer=bb_app_Millisecs();
												m_main_menu=false;
												m_levelComplete=false;
												m_selector=(m_stage-1)*8+m_level;
											}
										}else{
											if(m_stage==2){
												if(m_gameProgress==2){
													m_gameProgress=3;
													m_main_menu=false;
													m_levelComplete=false;
													m_gameProgressTimer=bb_app_Millisecs();
													m_selector=(m_stage-1)*8+m_level;
												}
											}else{
												if(m_stage==3){
													if(m_gameProgress==4){
														m_gameProgress=5;
														m_main_menu=false;
														m_levelComplete=false;
														m_gameProgressTimer=bb_app_Millisecs();
														m_selector=(m_stage-1)*8+24;
													}
												}
											}
										}
										if(m_stage<3){
											m_stage=m_stage+1;
											m_level=1;
											m_selector=(m_stage-1)*8+m_level;
										}else{
											m_stage=3;
											m_level=8;
											m_selector=(m_stage-1)*8+m_level;
											m_isPlaying=false;
										}
									}
									if(m_gameProgress!=1 && m_gameProgress!=3 && m_gameProgress!=5){
										p_LoadLevel(m_level);
										m_touchDelayStart=bb_app_Millisecs()+150;
									}
									m_touchDelayStart=bb_app_Millisecs()+150;
								}
								m_selector=1;
							}else{
								m_selector=9999;
							}
						}
					}else{
						m_selector=9999;
					}
					if(m_selector==9999){
						if(bb_autofit_VTouchX(0,true)>=FLOAT(25.0) && bb_autofit_VTouchX(0,true)<=FLOAT(200.0)){
							if(bb_autofit_VTouchY(0,true)>=FLOAT(25.0) && bb_autofit_VTouchY(0,true)<=FLOAT(200.0)){
								if((bb_input_TouchHit(0))!=0){
									m_isPlaying=true;
									m_levelComplete=false;
									p_ResetPlayer(m_level);
								}
								m_selector=4;
							}else{
								m_selector=9999;
							}
						}else{
							m_selector=9999;
						}
					}
					if(((bb_input_KeyHit(82))!=0) || ((bb_input_JoyHit(2,0))!=0)){
						m_isPlaying=true;
						m_levelComplete=false;
						p_ResetPlayer(m_level);
					}
					if(m_selector==9999){
						if(bb_autofit_VTouchX(0,true)>=FLOAT(1715.0) && bb_autofit_VTouchX(0,true)<=FLOAT(1885.0)){
							if(bb_autofit_VTouchY(0,true)>=FLOAT(25.0) && bb_autofit_VTouchY(0,true)<=FLOAT(80.0)){
								if((bb_input_TouchHit(0))!=0){
									m_levelComplete=false;
									m_isPlaying=false;
								}
								m_selector=5;
							}else{
								m_selector=9999;
							}
						}else{
							m_selector=9999;
						}
					}
					if(m_selector==9999){
						if(bb_autofit_VTouchX(0,true)>=FLOAT(922.0) && bb_autofit_VTouchX(0,true)<=FLOAT(997.0)){
							if(bb_autofit_VTouchY(0,true)>=FLOAT(291.0) && bb_autofit_VTouchY(0,true)<=FLOAT(351.0)){
								if((bb_input_TouchHit(0))!=0){
									String t_starText2=String(L"Stars",5);
									if(m_tStars==1){
										t_starText2=String(L"Star",4);
									}
									int t_levelNumber2=0;
									if(m_stage==1){
										t_levelNumber2=m_level;
									}else{
										t_levelNumber2=m_level+8*(m_stage-1);
									}
									c_DoTweet::m_LaunchTwitter(String(),String(L"I completed Level ",18)+String(t_levelNumber2)+String(L" and earned ",12)+String(m_tStars)+String(L" ",1)+t_starText2+String(L"!",1),String(L"ReboundGame",11));
									bbPrint(String(L"HERE",4));
								}
								m_selector=3;
							}else{
								m_selector=9999;
							}
						}else{
							m_selector=9999;
						}
					}
				}
			}else{
				if(m_options==true && m_credits==false){
					if(m_gamepad==true){
						if((bb_input_JoyHit(10,0))!=0){
							if(m_selector==4){
								m_selector=5;
							}else{
								if(m_selector==6){
									m_selector=7;
								}
							}
						}else{
							if((bb_input_JoyHit(8,0))!=0){
								if(m_selector==5){
									m_selector=4;
								}else{
									if(m_selector==7){
										m_selector=6;
									}
								}
							}else{
								if((bb_input_JoyHit(11,0))!=0){
									if(m_selector==1){
										m_selector=2;
									}else{
										if(m_selector==2){
											m_selector=3;
										}else{
											if(m_selector==3){
												m_selector=4;
											}else{
												if(m_selector==4){
													m_selector=6;
												}else{
													if(m_selector==6){
														m_selector=8;
													}else{
														if(m_selector==5){
															m_selector=7;
														}else{
															if(m_selector==7){
																m_selector=8;
															}else{
																if(m_selector==8){
																	m_selector=1;
																}
															}
														}
													}
												}
											}
										}
									}
								}else{
									if((bb_input_JoyHit(9,0))!=0){
										if(m_selector==1){
											m_selector=8;
										}else{
											if(m_selector==2){
												m_selector=1;
											}else{
												if(m_selector==3){
													m_selector=2;
												}else{
													if(m_selector==4){
														m_selector=3;
													}else{
														if(m_selector==6){
															m_selector=4;
														}else{
															if(m_selector==5){
																m_selector=3;
															}else{
																if(m_selector==7){
																	m_selector=5;
																}else{
																	if(m_selector==8){
																		m_selector=7;
																	}
																}
															}
														}
													}
												}
											}
										}
									}else{
										if((bb_input_JoyHit(0,0))!=0){
											if(m_selector==1){
												m_resX=1024;
												m_resY=576;
												p_SetResolution(1024,576,m_fullscreen);
											}else{
												if(m_selector==2){
													m_resX=1280;
													m_resY=720;
													p_SetResolution(1280,720,m_fullscreen);
												}else{
													if(m_selector==3){
														m_resX=1920;
														m_resY=1080;
														p_SetResolution(1920,1080,m_fullscreen);
													}else{
														if(m_selector==4){
															m_fullscreen=false;
															if(m_resX==1024 && m_resY==576){
																p_SetResolution(1024,576,m_fullscreen);
															}else{
																if(m_resX==1280 && m_resY==720){
																	p_SetResolution(1280,720,m_fullscreen);
																}else{
																	if(m_resX==1920 && m_resY==1080){
																		p_SetResolution(1920,1080,m_fullscreen);
																	}
																}
															}
														}else{
															if(m_selector==5){
																m_fullscreen=true;
																if(m_resX==1024 && m_resY==576){
																	p_SetResolution(1024,576,m_fullscreen);
																}else{
																	if(m_resX==1280 && m_resY==720){
																		p_SetResolution(1280,720,m_fullscreen);
																	}else{
																		if(m_resX==1920 && m_resY==1080){
																			p_SetResolution(1920,1080,m_fullscreen);
																		}
																	}
																}
															}else{
																if(m_selector==6){
																	m_credits=true;
																	m_img_level->p_Discard();
																	gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/UI/Credits1.png",24),1,c_Image::m_DefaultFlags));
																	m_creditStep=1;
																	m_creditStart=bb_app_Millisecs();
																}else{
																	if(m_selector==7){
																		bbError(String());
																	}else{
																		if(m_selector==8){
																			m_options=false;
																			if(m_main_menu==true){
																				m_selector=3;
																			}else{
																				m_selector=12;
																			}
																		}
																	}
																}
															}
														}
													}
												}
											}
										}else{
											if((bb_input_JoyHit(1,0))!=0){
												m_options=false;
												if(m_main_menu==true){
													m_selector=3;
												}else{
													m_selector=12;
												}
											}
										}
									}
								}
							}
						}
					}
					if(m_gamepad==false){
						if(bb_autofit_VTouchX(0,true)>=FLOAT(849.0) && bb_autofit_VTouchX(0,true)<=FLOAT(1079.0)){
							if(bb_autofit_VTouchY(0,true)>=FLOAT(233.0) && bb_autofit_VTouchY(0,true)<=FLOAT(313.0)){
								if((bb_input_TouchHit(0))!=0){
									m_resX=1024;
									m_resY=576;
									p_SetResolution(1024,576,m_fullscreen);
								}
								m_selector=1;
							}else{
								m_selector=9999;
							}
						}else{
							m_selector=9999;
						}
						if(m_selector==9999){
							if(bb_autofit_VTouchX(0,true)>=FLOAT(849.0) && bb_autofit_VTouchX(0,true)<=FLOAT(1079.0)){
								if(bb_autofit_VTouchY(0,true)>=FLOAT(329.0) && bb_autofit_VTouchY(0,true)<=FLOAT(412.0)){
									if((bb_input_TouchHit(0))!=0){
										m_resX=1280;
										m_resY=720;
										p_SetResolution(1280,720,m_fullscreen);
									}
									m_selector=2;
								}else{
									m_selector=9999;
								}
							}else{
								m_selector=9999;
							}
						}
						if(m_selector==9999){
							if(bb_autofit_VTouchX(0,true)>=FLOAT(849.0) && bb_autofit_VTouchX(0,true)<=FLOAT(1079.0)){
								if(bb_autofit_VTouchY(0,true)>=FLOAT(425.0) && bb_autofit_VTouchY(0,true)<=FLOAT(506.0)){
									if((bb_input_TouchHit(0))!=0){
										m_resX=1920;
										m_resY=1080;
										p_SetResolution(1920,1080,m_fullscreen);
									}
									m_selector=3;
								}else{
									m_selector=9999;
								}
							}else{
								m_selector=9999;
							}
						}
						if(m_selector==9999){
							if(bb_autofit_VTouchX(0,true)>=FLOAT(696.0) && bb_autofit_VTouchX(0,true)<=FLOAT(927.0)){
								if(bb_autofit_VTouchY(0,true)>=FLOAT(517.0) && bb_autofit_VTouchY(0,true)<=FLOAT(600.0)){
									if((bb_input_TouchHit(0))!=0){
										m_fullscreen=false;
										if(m_resX==1024 && m_resY==576){
											p_SetResolution(1024,576,m_fullscreen);
										}else{
											if(m_resX==1280 && m_resY==720){
												p_SetResolution(1280,720,m_fullscreen);
											}else{
												if(m_resX==1920 && m_resY==1080){
													p_SetResolution(1920,1080,m_fullscreen);
												}
											}
										}
									}
									m_selector=4;
								}else{
									m_selector=9999;
								}
							}else{
								m_selector=9999;
							}
						}
						if(m_selector==9999){
							if(bb_autofit_VTouchX(0,true)>=FLOAT(988.0) && bb_autofit_VTouchX(0,true)<=FLOAT(1221.0)){
								if(bb_autofit_VTouchY(0,true)>=FLOAT(517.0) && bb_autofit_VTouchY(0,true)<=FLOAT(600.0)){
									if((bb_input_TouchHit(0))!=0){
										m_fullscreen=true;
										if(m_resX==1024 && m_resY==576){
											p_SetResolution(1024,576,m_fullscreen);
										}else{
											if(m_resX==1280 && m_resY==720){
												p_SetResolution(1280,720,m_fullscreen);
											}else{
												if(m_resX==1920 && m_resY==1080){
													p_SetResolution(1920,1080,m_fullscreen);
												}
											}
										}
									}
									m_selector=5;
								}else{
									m_selector=9999;
								}
							}else{
								m_selector=9999;
							}
						}
						if(m_selector==9999){
							if(bb_autofit_VTouchX(0,true)>=FLOAT(696.0) && bb_autofit_VTouchX(0,true)<=FLOAT(927.0)){
								if(bb_autofit_VTouchY(0,true)>=FLOAT(629.0) && bb_autofit_VTouchY(0,true)<=FLOAT(713.0)){
									if((bb_input_TouchHit(0))!=0){
										m_credits=true;
										m_img_level->p_Discard();
										gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/UI/Credits1.png",24),1,c_Image::m_DefaultFlags));
										m_creditStep=1;
										m_creditStart=bb_app_Millisecs();
									}
									m_selector=6;
								}else{
									m_selector=9999;
								}
							}else{
								m_selector=9999;
							}
						}
						if(m_selector==9999){
							if(bb_autofit_VTouchX(0,true)>=FLOAT(988.0) && bb_autofit_VTouchX(0,true)<=FLOAT(1221.0)){
								if(bb_autofit_VTouchY(0,true)>=FLOAT(629.0) && bb_autofit_VTouchY(0,true)<=FLOAT(713.0)){
									if((bb_input_TouchHit(0))!=0){
										bbError(String());
									}
									m_selector=7;
								}else{
									m_selector=9999;
								}
							}else{
								m_selector=9999;
							}
						}
						if(m_selector==9999){
							if(bb_autofit_VTouchX(0,true)>=FLOAT(849.0) && bb_autofit_VTouchX(0,true)<=FLOAT(1079.0)){
								if(bb_autofit_VTouchY(0,true)>=FLOAT(747.0) && bb_autofit_VTouchY(0,true)<=FLOAT(828.0)){
									if((bb_input_TouchHit(0))!=0){
										m_options=false;
										if(m_main_menu==true){
											m_selector=3;
										}else{
											m_selector=12;
										}
									}
									m_selector=8;
								}else{
									m_selector=9999;
								}
							}else{
								m_selector=9999;
							}
						}
					}
				}else{
					if(m_credits==true){
						if(bb_app_Millisecs()>m_creditStart+4000){
							m_creditStep=m_creditStep+1;
							m_img_level->p_Discard();
							if(m_creditStep==2){
								gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/UI/Credits2.png",24),1,c_Image::m_DefaultFlags));
							}else{
								if(m_creditStep==3){
									gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/UI/Credits3.png",24),1,c_Image::m_DefaultFlags));
								}else{
									if(m_creditStep==4){
										gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/UI/Credits4.png",24),1,c_Image::m_DefaultFlags));
									}else{
										if(m_creditStep==5){
											gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/UI/Credits5.png",24),1,c_Image::m_DefaultFlags));
										}else{
											if(m_creditStep==6){
												gc_assign(m_img_level,bb_graphics_LoadImage(String(L"Graphics/UI/Credits6.png",24),1,c_Image::m_DefaultFlags));
											}
										}
									}
								}
							}
							if(m_creditStep>=7){
								m_creditStep=1;
								m_credits=false;
							}
							m_creditStart=bb_app_Millisecs();
						}
					}else{
						if(m_gamepad==true){
							if((bb_input_JoyHit(10,0))!=0){
								if(m_selector<4){
									m_selector=m_selector+1;
								}else{
									if(m_selector==4){
										if(m_stage<3){
											m_selector=10;
										}else{
											m_selector=5;
										}
									}else{
										if(m_selector>=5 && m_selector<8){
											m_selector=m_selector+1;
										}else{
											if(m_selector==8){
												if(m_stage<3){
													m_selector=10;
												}
											}else{
												if(m_selector==9){
													m_selector=1;
												}else{
													if(m_selector==11){
														m_selector=12;
													}else{
														if(m_selector==12){
															m_selector=11;
														}
													}
												}
											}
										}
									}
								}
							}else{
								if((bb_input_JoyHit(8,0))!=0){
									if(m_selector<=4){
										m_selector=m_selector-1;
										if(m_selector<1){
											if(m_stage>1){
												m_selector=9;
											}else{
												m_selector=1;
											}
										}
									}else{
										if(m_selector>=5 && m_selector<=8){
											m_selector=m_selector-1;
											if(m_selector==4){
												if(m_stage>1){
													m_selector=9;
												}
											}
										}else{
											if(m_selector==10){
												m_selector=8;
											}else{
												if(m_selector==12){
													m_selector=11;
												}else{
													if(m_selector==11){
														m_selector=12;
													}
												}
											}
										}
									}
								}else{
									if((bb_input_JoyHit(9,0))!=0){
										if(m_selector>=5 && m_selector<9){
											if(m_selector==5){
												m_selector=1;
											}else{
												if(m_selector==6){
													m_selector=2;
												}else{
													if(m_selector==7){
														m_selector=3;
													}else{
														if(m_selector==8){
															m_selector=4;
														}
													}
												}
											}
										}else{
											if(m_selector==11){
												m_selector=5;
											}else{
												if(m_selector==12){
													m_selector=8;
												}
											}
										}
									}else{
										if((bb_input_JoyHit(11,0))!=0){
											if(m_selector>=1 && m_selector<5){
												if(m_selector==1){
													m_selector=5;
												}else{
													if(m_selector==2){
														m_selector=6;
													}else{
														if(m_selector==3){
															m_selector=7;
														}else{
															if(m_selector==4){
																m_selector=8;
															}
														}
													}
												}
											}else{
												if(m_selector==5 || m_selector==6){
													m_selector=11;
												}else{
													if(m_selector==7 || m_selector==8){
														m_selector=12;
													}
												}
											}
										}
									}
								}
							}
							if((bb_input_JoyHit(0,0))!=0){
								if(m_selector>=1 && m_selector<=8){
									m_level=m_selector;
									if(m_gameData->m_stage[m_stage-1]->m_level[m_level-1]->m_unlocked==true){
										p_LoadLevel(m_level);
										m_selector=0;
									}
								}else{
									if(m_selector==9){
										m_stage=m_stage-1;
										if(m_stage<2){
											m_stage=1;
											m_selector=1;
										}
									}else{
										if(m_selector==10){
											m_stage=m_stage+1;
											if(m_stage>3){
												m_stage=3;
												m_selector=1;
											}
										}else{
											if(m_selector==11){
												m_main_menu=true;
												m_selector=1;
											}else{
												if(m_selector==12){
													m_options=true;
													m_selector=1;
												}
											}
										}
									}
								}
							}
							if((bb_input_JoyHit(1,0))!=0){
								m_main_menu=true;
								m_selector=1;
							}
						}
						if(m_gamepad==false){
							if(bb_autofit_VTouchY(0,true)>=FLOAT(84.0) && bb_autofit_VTouchY(0,true)<=FLOAT(386.0)){
								if(bb_autofit_VTouchX(0,true)>=FLOAT(216.0) && bb_autofit_VTouchX(0,true)<=FLOAT(430.0)){
									if((bb_input_TouchHit(0))!=0){
										m_level=1;
										if(m_gameData->m_stage[m_stage-1]->m_level[m_level-1]->m_unlocked==true){
											p_LoadLevel(m_level);
											m_touchDelayStart=bb_app_Millisecs()+150;
										}
									}
									m_selector=1;
								}else{
									if(bb_autofit_VTouchX(0,true)>=FLOAT(648.0) && bb_autofit_VTouchX(0,true)<=FLOAT(862.0)){
										if((bb_input_TouchHit(0))!=0){
											m_level=2;
											if(m_gameData->m_stage[m_stage-1]->m_level[m_level-1]->m_unlocked==true){
												p_LoadLevel(m_level);
												m_touchDelayStart=bb_app_Millisecs()+150;
											}
										}
										m_selector=2;
									}else{
										if(bb_autofit_VTouchX(0,true)>=FLOAT(1078.0) && bb_autofit_VTouchX(0,true)<=FLOAT(1292.0)){
											if((bb_input_TouchHit(0))!=0){
												m_level=3;
												if(m_gameData->m_stage[m_stage-1]->m_level[m_level-1]->m_unlocked==true){
													p_LoadLevel(m_level);
													m_touchDelayStart=bb_app_Millisecs()+150;
												}
											}
											m_selector=3;
										}else{
											if(bb_autofit_VTouchX(0,true)>=FLOAT(1512.0) && bb_autofit_VTouchX(0,true)<=FLOAT(1726.0)){
												if((bb_input_TouchHit(0))!=0){
													m_level=4;
													if(m_gameData->m_stage[m_stage-1]->m_level[m_level-1]->m_unlocked==true){
														p_LoadLevel(m_level);
														m_touchDelayStart=bb_app_Millisecs()+150;
													}
												}
												m_selector=4;
											}else{
												m_selector=9999;
											}
										}
									}
								}
							}else{
								if(bb_autofit_VTouchY(0,true)>=FLOAT(573.0) && bb_autofit_VTouchY(0,true)<=FLOAT(875.0)){
									if(bb_autofit_VTouchX(0,true)>=FLOAT(216.0) && bb_autofit_VTouchX(0,true)<=FLOAT(430.0)){
										if((bb_input_TouchHit(0))!=0){
											m_level=5;
											if(m_gameData->m_stage[m_stage-1]->m_level[m_level-1]->m_unlocked==true){
												p_LoadLevel(m_level);
												m_touchDelayStart=bb_app_Millisecs()+150;
											}
										}
										m_selector=5;
									}else{
										if(bb_autofit_VTouchX(0,true)>=FLOAT(648.0) && bb_autofit_VTouchX(0,true)<=FLOAT(862.0)){
											if((bb_input_TouchHit(0))!=0){
												m_level=6;
												if(m_gameData->m_stage[m_stage-1]->m_level[m_level-1]->m_unlocked==true){
													p_LoadLevel(m_level);
													m_touchDelayStart=bb_app_Millisecs()+150;
												}
											}
											m_selector=6;
										}else{
											if(bb_autofit_VTouchX(0,true)>=FLOAT(1078.0) && bb_autofit_VTouchX(0,true)<=FLOAT(1292.0)){
												if((bb_input_TouchHit(0))!=0){
													m_level=7;
													if(m_gameData->m_stage[m_stage-1]->m_level[m_level-1]->m_unlocked==true){
														p_LoadLevel(m_level);
														m_touchDelayStart=bb_app_Millisecs()+150;
													}
												}
												m_selector=7;
											}else{
												if(bb_autofit_VTouchX(0,true)>=FLOAT(1512.0) && bb_autofit_VTouchX(0,true)<=FLOAT(1726.0)){
													if((bb_input_TouchHit(0))!=0){
														m_level=8;
														if(m_gameData->m_stage[m_stage-1]->m_level[m_level-1]->m_unlocked==true){
															p_LoadLevel(m_level);
															m_touchDelayStart=bb_app_Millisecs()+150;
														}
													}
													m_selector=8;
												}else{
													m_selector=9999;
												}
											}
										}
									}
								}else{
									m_selector=9999;
								}
							}
							if(m_selector==9999){
								if(bb_autofit_VTouchX(0,true)>=FLOAT(1808.0) && bb_autofit_VTouchX(0,true)<=FLOAT(1885.0)){
									if(bb_autofit_VTouchY(0,true)>=FLOAT(413.0) && bb_autofit_VTouchY(0,true)<=FLOAT(530.0)){
										if((bb_input_TouchHit(0))!=0){
											m_stage=m_stage+1;
											if(m_stage>3){
												m_stage=3;
											}
										}
										m_selector=10;
									}else{
										m_selector=9999;
									}
								}
								if(bb_autofit_VTouchX(0,true)>=FLOAT(38.0) && bb_autofit_VTouchX(0,true)<=FLOAT(114.0)){
									if(bb_autofit_VTouchY(0,true)>=FLOAT(416.0) && bb_autofit_VTouchY(0,true)<=FLOAT(542.0)){
										if((bb_input_TouchHit(0))!=0){
											m_stage=m_stage-1;
											if(m_stage<1){
												m_stage=1;
											}
										}
										m_selector=9;
									}else{
										m_selector=9999;
									}
								}
								if(m_selector==9999){
									if(bb_autofit_VTouchX(0,true)>=FLOAT(0.0) && bb_autofit_VTouchX(0,true)<=FLOAT(380.0)){
										if(bb_autofit_VTouchY(0,true)>=FLOAT(940.0) && bb_autofit_VTouchY(0,true)<=FLOAT(1080.0)){
											if((bb_input_TouchHit(0))!=0){
												m_main_menu=true;
											}
											m_selector=11;
										}else{
											m_selector=9999;
										}
									}
									if(bb_autofit_VTouchX(0,true)>=FLOAT(1537.0) && bb_autofit_VTouchX(0,true)<=FLOAT(1920.0)){
										if(bb_autofit_VTouchY(0,true)>=FLOAT(940.0) && bb_autofit_VTouchY(0,true)<=FLOAT(1080.0)){
											if((bb_input_TouchHit(0))!=0){
												m_options=true;
												m_selector=1;
											}
											m_selector=12;
										}else{
											m_selector=9999;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	if(m_gamepad==false){
		if(((bb_input_JoyHit(10,0))!=0) || ((bb_input_JoyHit(8,0))!=0) || ((bb_input_JoyHit(9,0))!=0) || ((bb_input_JoyHit(11,0))!=0) || ((bb_input_JoyHit(0,0))!=0) || ((bb_input_JoyHit(1,0))!=0) || ((bb_input_JoyHit(2,0))!=0) || ((bb_input_JoyHit(3,0))!=0) || ((bb_input_JoyHit(7,0))!=0) || ((bb_input_JoyHit(6,0))!=0) || bb_input_JoyZ(0,0)<FLOAT(-0.01) || bb_input_JoyZ(0,0)>FLOAT(0.01)){
			m_selector=1;
			m_gamepad=true;
			m_mX=bb_autofit_VTouchX(0,true);
			m_mY=bb_autofit_VTouchY(0,true);
			bb_app_HideMouse();
		}
	}else{
		if(bb_autofit_VTouchX(0,true)>m_mX+FLOAT(5.0) || bb_autofit_VTouchX(0,true)<m_mX-FLOAT(5.0)){
			m_gamepad=false;
			m_selector=9999;
			bb_app_ShowMouse();
		}
		if(bb_autofit_VTouchY(0,true)>m_mY+FLOAT(5.0) || bb_autofit_VTouchX(0,true)<m_mY-FLOAT(5.0)){
			m_gamepad=false;
			m_selector=9999;
			bb_app_ShowMouse();
		}
	}
	if((bb_input_KeyHit(27))!=0){
		bbError(String());
	}
	return 0;
}
void c_Game::p_DrawLevel(int t_l){
	bb_graphics_DrawImage(m_img_level,FLOAT(0.0),FLOAT(0.0),0);
	if(m_levelComplete==false){
		if(m_selector==1){
			bb_graphics_DrawImage(m_img_redoH,FLOAT(25.0),FLOAT(25.0),0);
		}else{
			bb_graphics_DrawImage(m_img_redo,FLOAT(25.0),FLOAT(25.0),0);
		}
	}else{
		bb_graphics_DrawImage(m_img_redo,FLOAT(25.0),FLOAT(25.0),0);
	}
	bb_graphics_DrawImage(m_img_exit,m_exitX,m_exitY,0);
	if(m_levelComplete==false){
		if(m_selector==2){
			bb_graphics_DrawImage(m_img_menuH2,FLOAT(1715.0),FLOAT(25.0),0);
		}else{
			bb_graphics_DrawImage(m_img_menu,FLOAT(1715.0),FLOAT(25.0),0);
		}
	}else{
		bb_graphics_DrawImage(m_img_menu,FLOAT(1715.0),FLOAT(25.0),0);
	}
}
int c_Game::p_OnRender(){
	bb_autofit_UpdateVirtualDisplay(true,true);
	bb_graphics_Cls(FLOAT(255.0),FLOAT(255.0),FLOAT(255.0));
	if(m_isPlaying==true){
		if(m_tutorial==false){
			p_DrawLevel(m_level);
			String t_st=String((bb_app_Millisecs()-m_levelStartTimer)/1000)+String(L".",1);
			String t_mm=String(bb_app_Millisecs()-m_levelStartTimer);
			int t_len=t_mm.Length();
			t_st=t_st+t_mm.Slice(t_len-3,t_len-2);
			m_fnt_54Font->p_DrawText3(t_st,FLOAT(960.0),FLOAT(40.0),2);
			this->m_world->p_Render();
		}else{
			if(m_tutorial==true){
				if(m_tutStep==0){
					m_fnt_54Font->p_DrawText3(String(L"WELCOME TO REBOUND",18),FLOAT(960.0),FLOAT(540.0),2);
				}else{
					if(m_tutStep==1){
						m_fnt_54Font->p_DrawText3(String(L"LETS GET STARTED",16),FLOAT(960.0),FLOAT(540.0),2);
					}else{
						if(m_tutStep==2){
							m_fnt_54Font->p_DrawText3(String(L"THIS IS YOU",11),FLOAT(960.0),FLOAT(440.0),2);
							bb_graphics_DrawImage(m_img_player,FLOAT(960.0),FLOAT(640.0),0);
						}else{
							if(m_tutStep==3){
								m_fnt_54Font->p_DrawText3(String(L"YOU LIKE TO SPIN",16),FLOAT(960.0),FLOAT(440.0),2);
								bb_graphics_DrawImage(m_img_player,FLOAT(960.0),FLOAT(640.0),0);
							}else{
								if(m_tutStep==4){
									m_fnt_54Font->p_DrawText3(String(L"HOLD THE D KEY OR RIGHT ARROW KEY TO SPIN RIGHT",47),FLOAT(960.0),FLOAT(440.0),2);
								}else{
									if(m_tutStep==5){
										m_fnt_54Font->p_DrawText3(String(L"NICE WORK",9),FLOAT(960.0),FLOAT(440.0),2);
									}else{
										if(m_tutStep==6){
											m_fnt_54Font->p_DrawText3(String(L"HOLD THE A KEY OR LEFT ARROW KEY TO SPIN LEFT",45),FLOAT(960.0),FLOAT(440.0),2);
										}else{
											if(m_tutStep==7){
												m_fnt_54Font->p_DrawText3(String(L"GREAT",5),FLOAT(960.0),FLOAT(440.0),2);
											}else{
												if(m_tutStep==8){
													m_fnt_54Font->p_DrawText3(String(L"THIS IS HOW THE TYPICAL GAME SCREEN LOOKS",41),FLOAT(960.0),FLOAT(380.0),2);
												}else{
													if(m_tutStep==9){
														m_fnt_54Font->p_DrawText3(String(L"THE GOAL IS TO REACH THE EXIT",29),FLOAT(960.0),FLOAT(380.0),2);
													}else{
														if(m_tutStep==10){
															m_fnt_54Font->p_DrawText3(String(L"NOW LETS ADD SOME GRAVITY",25),FLOAT(960.0),FLOAT(380.0),2);
														}else{
															if(m_tutStep==11){
																m_fnt_54Font->p_DrawText3(String(L"TRY TO TILT YOURSELF TO REBOUND TOWARDS THE EXIT",48),FLOAT(960.0),FLOAT(380.0),2);
															}else{
																if(m_tutStep==12){
																	m_fnt_54Font->p_DrawText3(String(L"CLICK THE REDO BUTTON OR PRESS THE R KEY TO",43),FLOAT(960.0),FLOAT(380.0),2);
																	m_fnt_54Font->p_DrawText3(String(L"RESTART THE LEVEL",17),FLOAT(960.0),FLOAT(440.0),2);
																}else{
																	if(m_tutStep==13){
																		m_fnt_54Font->p_DrawText3(String(L"NOW REACH THE EXIT",18),FLOAT(960.0),FLOAT(380.0),2);
																	}else{
																		if(m_tutStep==14){
																			m_fnt_54Font->p_DrawText3(String(L"GREAT JOB",9),FLOAT(960.0),FLOAT(380.0),2);
																		}else{
																			if(m_tutStep==15){
																				m_fnt_54Font->p_DrawText3(String(L"THE FASTER YOU REACH THE EXIT THE MORE STARS",44),FLOAT(960.0),FLOAT(380.0),2);
																				m_fnt_54Font->p_DrawText3(String(L"YOU WILL EARN",13),FLOAT(960.0),FLOAT(440.0),2);
																			}else{
																				if(m_tutStep==16){
																					m_fnt_54Font->p_DrawText3(String(L"CLICK THE MENU BUTTON AT ANY TIME TO RETURN",43),FLOAT(960.0),FLOAT(380.0),2);
																					m_fnt_54Font->p_DrawText3(String(L"TO THE MENU",11),FLOAT(960.0),FLOAT(440.0),2);
																				}else{
																					if(m_tutStep==17){
																						m_fnt_54Font->p_DrawText3(String(L"THIS CONCLUDES THE TUTORIAL",27),FLOAT(960.0),FLOAT(540.0),2);
																					}else{
																						if(m_tutStep==18){
																							m_fnt_54Font->p_DrawText3(String(L"GOOD LUCK",9),FLOAT(960.0),FLOAT(540.0),2);
																						}
																					}
																				}
																			}
																		}
																	}
																}
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
				if(m_tutStep>=8 && m_tutStep<17){
					bb_graphics_DrawImage(m_img_level,FLOAT(0.0),FLOAT(0.0),0);
					if(m_selector==1){
						bb_graphics_DrawImage(m_img_redoH,FLOAT(25.0),FLOAT(25.0),0);
					}else{
						bb_graphics_DrawImage(m_img_redo,FLOAT(25.0),FLOAT(25.0),0);
					}
					bb_graphics_DrawImage(m_img_exit,m_exitX,m_exitY,0);
					if(m_selector==2){
						bb_graphics_DrawImage(m_img_menuH2,FLOAT(1715.0),FLOAT(25.0),0);
					}else{
						bb_graphics_DrawImage(m_img_menu,FLOAT(1715.0),FLOAT(25.0),0);
					}
				}
				this->m_world->p_Render();
			}
		}
	}else{
		if(m_main_menu==true && m_options==false && m_credits==false){
			bb_graphics_DrawImage(m_img_mainMenu,FLOAT(0.0),FLOAT(0.0),0);
			if(m_selector==1){
				bb_graphics_DrawImage(m_img_playH,FLOAT(708.0),FLOAT(647.0),0);
			}else{
				if(m_selector==2){
					bb_graphics_DrawImage(m_img_tutorialH,FLOAT(144.0),FLOAT(679.0),0);
				}else{
					if(m_selector==3){
						bb_graphics_DrawImage(m_img_optionsH2,FLOAT(1343.0),FLOAT(677.0),0);
					}
				}
			}
		}else{
			if(m_levelComplete==true && m_credits==false){
				p_DrawLevel(m_level);
				bb_graphics_DrawImage(m_img_levelComplete,FLOAT(960.0),FLOAT(470.0),0);
				if(m_selector==1){
					bb_graphics_DrawImage(m_img_nextH,FLOAT(981.0),FLOAT(700.0),0);
				}else{
					if(m_selector==2){
						bb_graphics_DrawImage(m_img_menuH3,FLOAT(702.0),FLOAT(700.0),0);
					}else{
						if(m_selector==3){
							bb_graphics_DrawImage(m_img_twitterH,FLOAT(922.0),FLOAT(291.0),0);
						}else{
							if(m_selector==4){
								bb_graphics_DrawImage(m_img_redoH,FLOAT(25.0),FLOAT(25.0),0);
							}else{
								if(m_selector==5){
									bb_graphics_DrawImage(m_img_menuH2,FLOAT(1715.0),FLOAT(25.0),0);
								}
							}
						}
					}
				}
				m_fnt_72Font->p_DrawText3(String(L"LEVEL ",6)+String(m_level+(m_stage-1)*8),FLOAT(960.0),FLOAT(110.0),2);
				m_fnt_timeFont->p_DrawText2(m_fTime,FLOAT(1005.0),FLOAT(378.0));
				int t_tt=int(m_gameData->m_stage[m_stage-1]->m_level[m_level-1]->m_bestTime);
				String t_st2=String(t_tt/1000)+String(L".",1);
				String t_mm2=String(t_tt);
				int t_len2=t_mm2.Length();
				t_st2=t_st2+t_mm2.Slice(t_len2-3,t_len2-2);
				m_fnt_timeFont->p_DrawText2(t_st2,FLOAT(1005.0),FLOAT(453.0));
				if(m_tStars==1){
					bb_graphics_DrawImage(m_img_starFullLarge,FLOAT(800.0),FLOAT(595.0),0);
				}else{
					if(m_tStars==2){
						bb_graphics_DrawImage(m_img_starFullLarge,FLOAT(800.0),FLOAT(595.0),0);
						bb_graphics_DrawImage(m_img_starFullLarge,FLOAT(915.0),FLOAT(595.0),0);
					}else{
						if(m_tStars==3){
							bb_graphics_DrawImage(m_img_starFullLarge,FLOAT(800.0),FLOAT(595.0),0);
							bb_graphics_DrawImage(m_img_starFullLarge,FLOAT(915.0),FLOAT(595.0),0);
							bb_graphics_DrawImage(m_img_starFullLarge,FLOAT(1033.0),FLOAT(595.0),0);
						}
					}
				}
			}else{
				if(m_options==true && m_credits==false){
					if(m_main_menu==true){
						bb_graphics_DrawImage(m_img_mainMenu,FLOAT(0.0),FLOAT(0.0),0);
					}else{
						bb_graphics_DrawImage(m_img_stageSelect,FLOAT(0.0),FLOAT(0.0),0);
						if(m_stage==1){
							bb_graphics_DrawRect(FLOAT(30.0),FLOAT(400.0),FLOAT(100.0),FLOAT(165.0));
						}else{
							if(m_stage==3){
								bb_graphics_DrawRect(FLOAT(1790.0),FLOAT(400.0),FLOAT(100.0),FLOAT(165.0));
							}
						}
						Float t_x=FLOAT(325.0);
						Float t_y=FLOAT(125.0);
						Float t_sx=FLOAT(243.0);
						Float t_sy=FLOAT(315.0);
						int t_add=(m_stage-1)*8;
						for(int t_i=0;t_i<=7;t_i=t_i+1){
							if(m_gameData->m_stage[m_stage-1]->m_level[t_i]->m_unlocked==true){
								bb_graphics_DrawRect(t_x-FLOAT(50.0),t_y+FLOAT(5.0),FLOAT(90.0),FLOAT(135.0));
								m_fnt_stageSelectFont->p_DrawText3(String(m_gameData->m_stage[m_stage-1]->m_level[t_i]->m_ID+1+t_add),t_x,t_y,2);
							}
							int t_stars=m_gameData->m_stage[m_stage-1]->m_level[t_i]->m_starsEarned;
							if(t_stars==1){
								bb_graphics_DrawImage(m_img_starFull,t_sx,t_sy,0);
							}else{
								if(t_stars==2){
									bb_graphics_DrawImage(m_img_starFull,t_sx,t_sy,0);
									bb_graphics_DrawImage(m_img_starFull,t_sx+FLOAT(58.0),t_sy,0);
								}else{
									if(t_stars==3){
										bb_graphics_DrawImage(m_img_starFull,t_sx,t_sy,0);
										bb_graphics_DrawImage(m_img_starFull,t_sx+FLOAT(58.0),t_sy,0);
										bb_graphics_DrawImage(m_img_starFull,t_sx+FLOAT(116.0),t_sy,0);
									}
								}
							}
							t_sx=t_sx+FLOAT(432.0);
							if(t_sx>FLOAT(1540.0)){
								t_sx=FLOAT(243.0);
								t_sy=FLOAT(804.0);
							}
							t_x=t_x+FLOAT(432.0);
							if(t_x>FLOAT(1621.0)){
								t_x=FLOAT(325.0);
								t_y=FLOAT(614.0);
							}
						}
					}
					bb_graphics_DrawImage(m_img_options,FLOAT(960.0),FLOAT(470.0),0);
					if(m_selector==1){
						bb_graphics_DrawImage(m_img_res1H,FLOAT(899.0),FLOAT(265.0),0);
					}else{
						if(m_selector==2){
							bb_graphics_DrawImage(m_img_res2H,FLOAT(899.0),FLOAT(362.0),0);
						}else{
							if(m_selector==3){
								bb_graphics_DrawImage(m_img_res3H,FLOAT(890.0),FLOAT(457.0),0);
							}else{
								if(m_selector==4){
									bb_graphics_DrawImage(m_img_windowedH,FLOAT(731.0),FLOAT(551.0),0);
								}else{
									if(m_selector==5){
										bb_graphics_DrawImage(m_img_fullscreenH,FLOAT(1017.0),FLOAT(551.0),0);
									}else{
										if(m_selector==6){
											bb_graphics_DrawImage(m_img_creditsH,FLOAT(739.0),FLOAT(662.0),0);
										}else{
											if(m_selector==7){
												bb_graphics_DrawImage(m_img_exitGameH,FLOAT(1013.0),FLOAT(662.0),0);
											}else{
												if(m_selector==8){
													bb_graphics_DrawImage(m_img_returnH,FLOAT(894.0),FLOAT(778.0),0);
												}
											}
										}
									}
								}
							}
						}
					}
				}else{
					if(m_credits==true){
						bb_graphics_DrawImage(m_img_level,FLOAT(0.0),FLOAT(0.0),0);
					}else{
						if(m_gameProgress==1){
							bb_graphics_DrawImage(m_img_stage2,FLOAT(0.0),FLOAT(0.0),0);
							if(bb_app_Millisecs()>=m_gameProgressTimer+3500){
								m_gameProgress=2;
							}
						}else{
							if(m_gameProgress==3){
								bb_graphics_DrawImage(m_img_stage3,FLOAT(0.0),FLOAT(0.0),0);
								if(bb_app_Millisecs()>=m_gameProgressTimer+3500){
									m_gameProgress=4;
								}
							}else{
								if(m_gameProgress==5){
									bb_graphics_DrawImage(m_img_completed,FLOAT(0.0),FLOAT(0.0),0);
									if(bb_app_Millisecs()>=m_gameProgressTimer+3500){
										m_gameProgress=6;
									}
								}else{
									bb_graphics_DrawImage(m_img_stageSelect,FLOAT(0.0),FLOAT(0.0),0);
									if(m_selector!=9999){
										Float t_x2=FLOAT(0.0);
										Float t_y2=FLOAT(0.0);
										if(m_selector==1){
											t_x2=FLOAT(218.0);
											t_y2=FLOAT(84.0);
										}else{
											if(m_selector==2){
												t_x2=FLOAT(650.0);
												t_y2=FLOAT(84.0);
											}else{
												if(m_selector==3){
													t_x2=FLOAT(1080.0);
													t_y2=FLOAT(84.0);
												}else{
													if(m_selector==4){
														t_x2=FLOAT(1514.0);
														t_y2=FLOAT(84.0);
													}else{
														if(m_selector==5){
															t_x2=FLOAT(216.0);
															t_y2=FLOAT(573.0);
														}else{
															if(m_selector==6){
																t_x2=FLOAT(648.0);
																t_y2=FLOAT(573.0);
															}else{
																if(m_selector==7){
																	t_x2=FLOAT(1080.0);
																	t_y2=FLOAT(573.0);
																}else{
																	if(m_selector==8){
																		t_x2=FLOAT(1512.0);
																		t_y2=FLOAT(573.0);
																	}else{
																		if(m_selector==9){
																			t_x2=FLOAT(40.0);
																			t_y2=FLOAT(417.0);
																		}else{
																			if(m_selector==10){
																				t_x2=FLOAT(1808.0);
																				t_y2=FLOAT(417.0);
																			}else{
																				if(m_selector==11){
																					t_x2=FLOAT(0.0);
																					t_y2=FLOAT(943.0);
																				}else{
																					if(m_selector==12){
																						t_x2=FLOAT(1540.0);
																						t_y2=FLOAT(943.0);
																					}
																				}
																			}
																		}
																	}
																}
															}
														}
													}
												}
											}
										}
										if(m_selector<9){
											bb_graphics_DrawImage(m_img_levelH,t_x2,t_y2,0);
										}else{
											if(m_selector==9){
												bb_graphics_DrawImage(m_img_leftArrowH,t_x2,t_y2,0);
											}else{
												if(m_selector==10){
													bb_graphics_DrawImage(m_img_rightArrowH,t_x2,t_y2,0);
												}else{
													if(m_selector==11){
														bb_graphics_DrawImage(m_img_menuH1,t_x2,t_y2,0);
													}else{
														if(m_selector==12){
															bb_graphics_DrawImage(m_img_optionsH1,t_x2,t_y2,0);
														}
													}
												}
											}
										}
									}
									if(m_stage==1){
										bb_graphics_DrawRect(FLOAT(30.0),FLOAT(400.0),FLOAT(100.0),FLOAT(165.0));
									}else{
										if(m_stage==3){
											bb_graphics_DrawRect(FLOAT(1790.0),FLOAT(400.0),FLOAT(100.0),FLOAT(165.0));
										}
									}
									Float t_x3=FLOAT(325.0);
									Float t_y3=FLOAT(125.0);
									Float t_sx2=FLOAT(243.0);
									Float t_sy2=FLOAT(315.0);
									int t_add2=(m_stage-1)*8;
									for(int t_i2=0;t_i2<=7;t_i2=t_i2+1){
										if(m_gameData->m_stage[m_stage-1]->m_level[t_i2]->m_unlocked==true){
											bb_graphics_DrawRect(t_x3-FLOAT(50.0),t_y3+FLOAT(5.0),FLOAT(90.0),FLOAT(135.0));
											if(m_selector!=t_i2+1){
												m_fnt_stageSelectFont->p_DrawText3(String(m_gameData->m_stage[m_stage-1]->m_level[t_i2]->m_ID+1+t_add2),t_x3,t_y3,2);
											}else{
												m_fnt_stageSelectFontH->p_DrawText3(String(m_gameData->m_stage[m_stage-1]->m_level[t_i2]->m_ID+1+t_add2),t_x3,t_y3,2);
											}
										}
										int t_stars2=m_gameData->m_stage[m_stage-1]->m_level[t_i2]->m_starsEarned;
										if(m_selector!=t_i2+1){
											if(t_stars2==1){
												bb_graphics_DrawImage(m_img_starFull,t_sx2,t_sy2,0);
											}else{
												if(t_stars2==2){
													bb_graphics_DrawImage(m_img_starFull,t_sx2,t_sy2,0);
													bb_graphics_DrawImage(m_img_starFull,t_sx2+FLOAT(58.0),t_sy2,0);
												}else{
													if(t_stars2==3){
														bb_graphics_DrawImage(m_img_starFull,t_sx2,t_sy2,0);
														bb_graphics_DrawImage(m_img_starFull,t_sx2+FLOAT(58.0),t_sy2,0);
														bb_graphics_DrawImage(m_img_starFull,t_sx2+FLOAT(116.0),t_sy2,0);
													}
												}
											}
										}else{
											if(t_stars2==1){
												bb_graphics_DrawImage(m_img_starFullH,t_sx2,t_sy2,0);
											}else{
												if(t_stars2==2){
													bb_graphics_DrawImage(m_img_starFullH,t_sx2,t_sy2,0);
													bb_graphics_DrawImage(m_img_starFullH,t_sx2+FLOAT(58.0),t_sy2,0);
												}else{
													if(t_stars2==3){
														bb_graphics_DrawImage(m_img_starFullH,t_sx2,t_sy2,0);
														bb_graphics_DrawImage(m_img_starFullH,t_sx2+FLOAT(58.0),t_sy2,0);
														bb_graphics_DrawImage(m_img_starFullH,t_sx2+FLOAT(116.0),t_sy2,0);
													}
												}
											}
										}
										t_sx2=t_sx2+FLOAT(432.0);
										if(t_sx2>FLOAT(1540.0)){
											t_sx2=FLOAT(243.0);
											t_sy2=FLOAT(804.0);
										}
										t_x3=t_x3+FLOAT(432.0);
										if(t_x3>FLOAT(1621.0)){
											t_x3=FLOAT(325.0);
											t_y3=FLOAT(614.0);
										}
									}
									m_fnt_stageFont->p_DrawText3(String(L"STAGE ",6)+String(m_stage),FLOAT(960.0),FLOAT(435.0),2);
								}
							}
						}
					}
				}
			}
		}
	}
	return 0;
}
void c_Game::mark(){
	c_App::mark();
	gc_mark_q(m_img_player);
	gc_mark_q(m_img_mainMenu);
	gc_mark_q(m_img_stageSelect);
	gc_mark_q(m_img_levelComplete);
	gc_mark_q(m_img_level);
	gc_mark_q(m_img_redo);
	gc_mark_q(m_img_menu);
	gc_mark_q(m_img_exit);
	gc_mark_q(m_img_starFull);
	gc_mark_q(m_img_starFullLarge);
	gc_mark_q(m_img_levelH);
	gc_mark_q(m_img_starFullH);
	gc_mark_q(m_img_rightArrowH);
	gc_mark_q(m_img_leftArrowH);
	gc_mark_q(m_img_menuH1);
	gc_mark_q(m_img_optionsH1);
	gc_mark_q(m_img_playH);
	gc_mark_q(m_img_tutorialH);
	gc_mark_q(m_img_optionsH2);
	gc_mark_q(m_img_menuH2);
	gc_mark_q(m_img_menuH3);
	gc_mark_q(m_img_nextH);
	gc_mark_q(m_img_twitterH);
	gc_mark_q(m_img_redoH);
	gc_mark_q(m_img_barrierV);
	gc_mark_q(m_img_barrierH);
	gc_mark_q(m_img_cross1);
	gc_mark_q(m_img_cube1);
	gc_mark_q(m_img_options);
	gc_mark_q(m_img_res1H);
	gc_mark_q(m_img_res2H);
	gc_mark_q(m_img_res3H);
	gc_mark_q(m_img_windowedH);
	gc_mark_q(m_img_fullscreenH);
	gc_mark_q(m_img_creditsH);
	gc_mark_q(m_img_exitGameH);
	gc_mark_q(m_img_returnH);
	gc_mark_q(m_img_stage2);
	gc_mark_q(m_img_stage3);
	gc_mark_q(m_img_completed);
	gc_mark_q(m_img_androidOptions);
	gc_mark_q(m_img_on);
	gc_mark_q(m_img_off);
	gc_mark_q(m_fnt_stageFont);
	gc_mark_q(m_fnt_stageSelectFont);
	gc_mark_q(m_fnt_stageSelectFontH);
	gc_mark_q(m_fnt_72Font);
	gc_mark_q(m_fnt_54Font);
	gc_mark_q(m_fnt_timeFont);
	gc_mark_q(m_world);
	gc_mark_q(m_barrierList);
	gc_mark_q(m_gameData);
	gc_mark_q(m_player);
}
c_App* bb_app__app;
c_GameDelegate::c_GameDelegate(){
	m__graphics=0;
	m__audio=0;
	m__input=0;
}
c_GameDelegate* c_GameDelegate::m_new(){
	return this;
}
void c_GameDelegate::StartGame(){
	gc_assign(m__graphics,(new gxtkGraphics));
	bb_graphics_SetGraphicsDevice(m__graphics);
	bb_graphics_SetFont(0,32);
	gc_assign(m__audio,(new gxtkAudio));
	bb_audio_SetAudioDevice(m__audio);
	gc_assign(m__input,(new c_InputDevice)->m_new());
	bb_input_SetInputDevice(m__input);
	bb_app_ValidateDeviceWindow(false);
	bb_app_EnumDisplayModes();
	bb_app__app->p_OnCreate();
}
void c_GameDelegate::SuspendGame(){
	bb_app__app->p_OnSuspend();
	m__audio->Suspend();
}
void c_GameDelegate::ResumeGame(){
	m__audio->Resume();
	bb_app__app->p_OnResume();
}
void c_GameDelegate::UpdateGame(){
	bb_app_ValidateDeviceWindow(true);
	m__input->p_BeginUpdate();
	bb_app__app->p_OnUpdate();
	m__input->p_EndUpdate();
}
void c_GameDelegate::RenderGame(){
	bb_app_ValidateDeviceWindow(true);
	int t_mode=m__graphics->BeginRender();
	if((t_mode)!=0){
		bb_graphics_BeginRender();
	}
	if(t_mode==2){
		bb_app__app->p_OnLoading();
	}else{
		bb_app__app->p_OnRender();
	}
	if((t_mode)!=0){
		bb_graphics_EndRender();
	}
	m__graphics->EndRender();
}
void c_GameDelegate::KeyEvent(int t_event,int t_data){
	m__input->p_KeyEvent(t_event,t_data);
	if(t_event!=1){
		return;
	}
	int t_1=t_data;
	if(t_1==432){
		bb_app__app->p_OnClose();
	}else{
		if(t_1==416){
			bb_app__app->p_OnBack();
		}
	}
}
void c_GameDelegate::MouseEvent(int t_event,int t_data,Float t_x,Float t_y){
	m__input->p_MouseEvent(t_event,t_data,t_x,t_y);
}
void c_GameDelegate::TouchEvent(int t_event,int t_data,Float t_x,Float t_y){
	m__input->p_TouchEvent(t_event,t_data,t_x,t_y);
}
void c_GameDelegate::MotionEvent(int t_event,int t_data,Float t_x,Float t_y,Float t_z){
	m__input->p_MotionEvent(t_event,t_data,t_x,t_y,t_z);
}
void c_GameDelegate::DiscardGraphics(){
	m__graphics->DiscardGraphics();
}
void c_GameDelegate::mark(){
	BBGameDelegate::mark();
	gc_mark_q(m__graphics);
	gc_mark_q(m__audio);
	gc_mark_q(m__input);
}
c_GameDelegate* bb_app__delegate;
BBGame* bb_app__game;
int bbMain(){
	(new c_Game)->m_new();
	return 0;
}
gxtkGraphics* bb_graphics_device;
int bb_graphics_SetGraphicsDevice(gxtkGraphics* t_dev){
	gc_assign(bb_graphics_device,t_dev);
	return 0;
}
c_Image::c_Image(){
	m_surface=0;
	m_width=0;
	m_height=0;
	m_frames=Array<c_Frame* >();
	m_flags=0;
	m_tx=FLOAT(.0);
	m_ty=FLOAT(.0);
	m_source=0;
}
int c_Image::m_DefaultFlags;
c_Image* c_Image::m_new(){
	return this;
}
int c_Image::p_SetHandle(Float t_tx,Float t_ty){
	this->m_tx=t_tx;
	this->m_ty=t_ty;
	this->m_flags=this->m_flags&-2;
	return 0;
}
int c_Image::p_ApplyFlags(int t_iflags){
	m_flags=t_iflags;
	if((m_flags&2)!=0){
		Array<c_Frame* > t_=m_frames;
		int t_2=0;
		while(t_2<t_.Length()){
			c_Frame* t_f=t_[t_2];
			t_2=t_2+1;
			t_f->m_x+=1;
		}
		m_width-=2;
	}
	if((m_flags&4)!=0){
		Array<c_Frame* > t_3=m_frames;
		int t_4=0;
		while(t_4<t_3.Length()){
			c_Frame* t_f2=t_3[t_4];
			t_4=t_4+1;
			t_f2->m_y+=1;
		}
		m_height-=2;
	}
	if((m_flags&1)!=0){
		p_SetHandle(Float(m_width)/FLOAT(2.0),Float(m_height)/FLOAT(2.0));
	}
	if(m_frames.Length()==1 && m_frames[0]->m_x==0 && m_frames[0]->m_y==0 && m_width==m_surface->Width() && m_height==m_surface->Height()){
		m_flags|=65536;
	}
	return 0;
}
c_Image* c_Image::p_Init(gxtkSurface* t_surf,int t_nframes,int t_iflags){
	if((m_surface)!=0){
		bbError(String(L"Image already initialized",25));
	}
	gc_assign(m_surface,t_surf);
	m_width=m_surface->Width()/t_nframes;
	m_height=m_surface->Height();
	gc_assign(m_frames,Array<c_Frame* >(t_nframes));
	for(int t_i=0;t_i<t_nframes;t_i=t_i+1){
		gc_assign(m_frames[t_i],(new c_Frame)->m_new(t_i*m_width,0));
	}
	p_ApplyFlags(t_iflags);
	return this;
}
c_Image* c_Image::p_Init2(gxtkSurface* t_surf,int t_x,int t_y,int t_iwidth,int t_iheight,int t_nframes,int t_iflags,c_Image* t_src,int t_srcx,int t_srcy,int t_srcw,int t_srch){
	if((m_surface)!=0){
		bbError(String(L"Image already initialized",25));
	}
	gc_assign(m_surface,t_surf);
	gc_assign(m_source,t_src);
	m_width=t_iwidth;
	m_height=t_iheight;
	gc_assign(m_frames,Array<c_Frame* >(t_nframes));
	int t_ix=t_x;
	int t_iy=t_y;
	for(int t_i=0;t_i<t_nframes;t_i=t_i+1){
		if(t_ix+m_width>t_srcw){
			t_ix=0;
			t_iy+=m_height;
		}
		if(t_ix+m_width>t_srcw || t_iy+m_height>t_srch){
			bbError(String(L"Image frame outside surface",27));
		}
		gc_assign(m_frames[t_i],(new c_Frame)->m_new(t_ix+t_srcx,t_iy+t_srcy));
		t_ix+=m_width;
	}
	p_ApplyFlags(t_iflags);
	return this;
}
int c_Image::p_Width(){
	return m_width;
}
int c_Image::p_Height(){
	return m_height;
}
int c_Image::p_Discard(){
	if(((m_surface)!=0) && !((m_source)!=0)){
		m_surface->Discard();
		m_surface=0;
	}
	return 0;
}
void c_Image::mark(){
	Object::mark();
	gc_mark_q(m_surface);
	gc_mark_q(m_frames);
	gc_mark_q(m_source);
}
c_GraphicsContext::c_GraphicsContext(){
	m_defaultFont=0;
	m_font=0;
	m_firstChar=0;
	m_matrixSp=0;
	m_ix=FLOAT(1.0);
	m_iy=FLOAT(.0);
	m_jx=FLOAT(.0);
	m_jy=FLOAT(1.0);
	m_tx=FLOAT(.0);
	m_ty=FLOAT(.0);
	m_tformed=0;
	m_matDirty=0;
	m_color_r=FLOAT(.0);
	m_color_g=FLOAT(.0);
	m_color_b=FLOAT(.0);
	m_alpha=FLOAT(.0);
	m_blend=0;
	m_scissor_x=FLOAT(.0);
	m_scissor_y=FLOAT(.0);
	m_scissor_width=FLOAT(.0);
	m_scissor_height=FLOAT(.0);
	m_matrixStack=Array<Float >(192);
}
c_GraphicsContext* c_GraphicsContext::m_new(){
	return this;
}
int c_GraphicsContext::p_Validate(){
	if((m_matDirty)!=0){
		bb_graphics_renderDevice->SetMatrix(bb_graphics_context->m_ix,bb_graphics_context->m_iy,bb_graphics_context->m_jx,bb_graphics_context->m_jy,bb_graphics_context->m_tx,bb_graphics_context->m_ty);
		m_matDirty=0;
	}
	return 0;
}
void c_GraphicsContext::mark(){
	Object::mark();
	gc_mark_q(m_defaultFont);
	gc_mark_q(m_font);
	gc_mark_q(m_matrixStack);
}
c_GraphicsContext* bb_graphics_context;
String bb_data_FixDataPath(String t_path){
	int t_i=t_path.Find(String(L":/",2),0);
	if(t_i!=-1 && t_path.Find(String(L"/",1),0)==t_i+1){
		return t_path;
	}
	if(t_path.StartsWith(String(L"./",2)) || t_path.StartsWith(String(L"/",1))){
		return t_path;
	}
	return String(L"monkey://data/",14)+t_path;
}
c_Frame::c_Frame(){
	m_x=0;
	m_y=0;
}
c_Frame* c_Frame::m_new(int t_x,int t_y){
	this->m_x=t_x;
	this->m_y=t_y;
	return this;
}
c_Frame* c_Frame::m_new2(){
	return this;
}
void c_Frame::mark(){
	Object::mark();
}
c_Image* bb_graphics_LoadImage(String t_path,int t_frameCount,int t_flags){
	gxtkSurface* t_surf=bb_graphics_device->LoadSurface(bb_data_FixDataPath(t_path));
	if((t_surf)!=0){
		return ((new c_Image)->m_new())->p_Init(t_surf,t_frameCount,t_flags);
	}
	return 0;
}
c_Image* bb_graphics_LoadImage2(String t_path,int t_frameWidth,int t_frameHeight,int t_frameCount,int t_flags){
	gxtkSurface* t_surf=bb_graphics_device->LoadSurface(bb_data_FixDataPath(t_path));
	if((t_surf)!=0){
		return ((new c_Image)->m_new())->p_Init2(t_surf,0,0,t_frameWidth,t_frameHeight,t_frameCount,t_flags,0,0,0,t_surf->Width(),t_surf->Height());
	}
	return 0;
}
int bb_graphics_SetFont(c_Image* t_font,int t_firstChar){
	if(!((t_font)!=0)){
		if(!((bb_graphics_context->m_defaultFont)!=0)){
			gc_assign(bb_graphics_context->m_defaultFont,bb_graphics_LoadImage(String(L"mojo_font.png",13),96,2));
		}
		t_font=bb_graphics_context->m_defaultFont;
		t_firstChar=32;
	}
	gc_assign(bb_graphics_context->m_font,t_font);
	bb_graphics_context->m_firstChar=t_firstChar;
	return 0;
}
gxtkAudio* bb_audio_device;
int bb_audio_SetAudioDevice(gxtkAudio* t_dev){
	gc_assign(bb_audio_device,t_dev);
	return 0;
}
c_InputDevice::c_InputDevice(){
	m__joyStates=Array<c_JoyState* >(4);
	m__keyDown=Array<bool >(512);
	m__keyHitPut=0;
	m__keyHitQueue=Array<int >(33);
	m__keyHit=Array<int >(512);
	m__charGet=0;
	m__charPut=0;
	m__charQueue=Array<int >(32);
	m__mouseX=FLOAT(.0);
	m__mouseY=FLOAT(.0);
	m__touchX=Array<Float >(32);
	m__touchY=Array<Float >(32);
	m__accelX=FLOAT(.0);
	m__accelY=FLOAT(.0);
	m__accelZ=FLOAT(.0);
}
c_InputDevice* c_InputDevice::m_new(){
	for(int t_i=0;t_i<4;t_i=t_i+1){
		gc_assign(m__joyStates[t_i],(new c_JoyState)->m_new());
	}
	return this;
}
void c_InputDevice::p_PutKeyHit(int t_key){
	if(m__keyHitPut==m__keyHitQueue.Length()){
		return;
	}
	m__keyHit[t_key]+=1;
	m__keyHitQueue[m__keyHitPut]=t_key;
	m__keyHitPut+=1;
}
void c_InputDevice::p_BeginUpdate(){
	for(int t_i=0;t_i<4;t_i=t_i+1){
		c_JoyState* t_state=m__joyStates[t_i];
		if(!BBGame::Game()->PollJoystick(t_i,t_state->m_joyx,t_state->m_joyy,t_state->m_joyz,t_state->m_buttons)){
			break;
		}
		for(int t_j=0;t_j<32;t_j=t_j+1){
			int t_key=256+t_i*32+t_j;
			if(t_state->m_buttons[t_j]){
				if(!m__keyDown[t_key]){
					m__keyDown[t_key]=true;
					p_PutKeyHit(t_key);
				}
			}else{
				m__keyDown[t_key]=false;
			}
		}
	}
}
void c_InputDevice::p_EndUpdate(){
	for(int t_i=0;t_i<m__keyHitPut;t_i=t_i+1){
		m__keyHit[m__keyHitQueue[t_i]]=0;
	}
	m__keyHitPut=0;
	m__charGet=0;
	m__charPut=0;
}
void c_InputDevice::p_KeyEvent(int t_event,int t_data){
	int t_1=t_event;
	if(t_1==1){
		if(!m__keyDown[t_data]){
			m__keyDown[t_data]=true;
			p_PutKeyHit(t_data);
			if(t_data==1){
				m__keyDown[384]=true;
				p_PutKeyHit(384);
			}else{
				if(t_data==384){
					m__keyDown[1]=true;
					p_PutKeyHit(1);
				}
			}
		}
	}else{
		if(t_1==2){
			if(m__keyDown[t_data]){
				m__keyDown[t_data]=false;
				if(t_data==1){
					m__keyDown[384]=false;
				}else{
					if(t_data==384){
						m__keyDown[1]=false;
					}
				}
			}
		}else{
			if(t_1==3){
				if(m__charPut<m__charQueue.Length()){
					m__charQueue[m__charPut]=t_data;
					m__charPut+=1;
				}
			}
		}
	}
}
void c_InputDevice::p_MouseEvent(int t_event,int t_data,Float t_x,Float t_y){
	int t_2=t_event;
	if(t_2==4){
		p_KeyEvent(1,1+t_data);
	}else{
		if(t_2==5){
			p_KeyEvent(2,1+t_data);
			return;
		}else{
			if(t_2==6){
			}else{
				return;
			}
		}
	}
	m__mouseX=t_x;
	m__mouseY=t_y;
	m__touchX[0]=t_x;
	m__touchY[0]=t_y;
}
void c_InputDevice::p_TouchEvent(int t_event,int t_data,Float t_x,Float t_y){
	int t_3=t_event;
	if(t_3==7){
		p_KeyEvent(1,384+t_data);
	}else{
		if(t_3==8){
			p_KeyEvent(2,384+t_data);
			return;
		}else{
			if(t_3==9){
			}else{
				return;
			}
		}
	}
	m__touchX[t_data]=t_x;
	m__touchY[t_data]=t_y;
	if(t_data==0){
		m__mouseX=t_x;
		m__mouseY=t_y;
	}
}
void c_InputDevice::p_MotionEvent(int t_event,int t_data,Float t_x,Float t_y,Float t_z){
	int t_4=t_event;
	if(t_4==10){
	}else{
		return;
	}
	m__accelX=t_x;
	m__accelY=t_y;
	m__accelZ=t_z;
}
Float c_InputDevice::p_TouchX(int t_index){
	if(t_index>=0 && t_index<32){
		return m__touchX[t_index];
	}
	return FLOAT(0.0);
}
Float c_InputDevice::p_TouchY(int t_index){
	if(t_index>=0 && t_index<32){
		return m__touchY[t_index];
	}
	return FLOAT(0.0);
}
bool c_InputDevice::p_KeyDown(int t_key){
	if(t_key>0 && t_key<512){
		return m__keyDown[t_key];
	}
	return false;
}
Float c_InputDevice::p_JoyZ(int t_index,int t_unit){
	return m__joyStates[t_unit]->m_joyz[t_index];
}
int c_InputDevice::p_KeyHit(int t_key){
	if(t_key>0 && t_key<512){
		return m__keyHit[t_key];
	}
	return 0;
}
void c_InputDevice::mark(){
	Object::mark();
	gc_mark_q(m__joyStates);
	gc_mark_q(m__keyDown);
	gc_mark_q(m__keyHitQueue);
	gc_mark_q(m__keyHit);
	gc_mark_q(m__charQueue);
	gc_mark_q(m__touchX);
	gc_mark_q(m__touchY);
}
c_JoyState::c_JoyState(){
	m_joyx=Array<Float >(2);
	m_joyy=Array<Float >(2);
	m_joyz=Array<Float >(2);
	m_buttons=Array<bool >(32);
}
c_JoyState* c_JoyState::m_new(){
	return this;
}
void c_JoyState::mark(){
	Object::mark();
	gc_mark_q(m_joyx);
	gc_mark_q(m_joyy);
	gc_mark_q(m_joyz);
	gc_mark_q(m_buttons);
}
c_InputDevice* bb_input_device;
int bb_input_SetInputDevice(c_InputDevice* t_dev){
	gc_assign(bb_input_device,t_dev);
	return 0;
}
int bb_app__devWidth;
int bb_app__devHeight;
void bb_app_ValidateDeviceWindow(bool t_notifyApp){
	int t_w=bb_app__game->GetDeviceWidth();
	int t_h=bb_app__game->GetDeviceHeight();
	if(t_w==bb_app__devWidth && t_h==bb_app__devHeight){
		return;
	}
	bb_app__devWidth=t_w;
	bb_app__devHeight=t_h;
	if(t_notifyApp){
		bb_app__app->p_OnResize();
	}
}
c_DisplayMode::c_DisplayMode(){
	m__width=0;
	m__height=0;
}
c_DisplayMode* c_DisplayMode::m_new(int t_width,int t_height){
	m__width=t_width;
	m__height=t_height;
	return this;
}
c_DisplayMode* c_DisplayMode::m_new2(){
	return this;
}
void c_DisplayMode::mark(){
	Object::mark();
}
c_Map::c_Map(){
	m_root=0;
}
c_Map* c_Map::m_new(){
	return this;
}
c_Node* c_Map::p_FindNode(int t_key){
	c_Node* t_node=m_root;
	while((t_node)!=0){
		int t_cmp=p_Compare(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				return t_node;
			}
		}
	}
	return t_node;
}
bool c_Map::p_Contains(int t_key){
	return p_FindNode(t_key)!=0;
}
int c_Map::p_RotateLeft(c_Node* t_node){
	c_Node* t_child=t_node->m_right;
	gc_assign(t_node->m_right,t_child->m_left);
	if((t_child->m_left)!=0){
		gc_assign(t_child->m_left->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_left){
			gc_assign(t_node->m_parent->m_left,t_child);
		}else{
			gc_assign(t_node->m_parent->m_right,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_left,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map::p_RotateRight(c_Node* t_node){
	c_Node* t_child=t_node->m_left;
	gc_assign(t_node->m_left,t_child->m_right);
	if((t_child->m_right)!=0){
		gc_assign(t_child->m_right->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_right){
			gc_assign(t_node->m_parent->m_right,t_child);
		}else{
			gc_assign(t_node->m_parent->m_left,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_right,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map::p_InsertFixup(c_Node* t_node){
	while(((t_node->m_parent)!=0) && t_node->m_parent->m_color==-1 && ((t_node->m_parent->m_parent)!=0)){
		if(t_node->m_parent==t_node->m_parent->m_parent->m_left){
			c_Node* t_uncle=t_node->m_parent->m_parent->m_right;
			if(((t_uncle)!=0) && t_uncle->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle->m_color=1;
				t_uncle->m_parent->m_color=-1;
				t_node=t_uncle->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_right){
					t_node=t_node->m_parent;
					p_RotateLeft(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateRight(t_node->m_parent->m_parent);
			}
		}else{
			c_Node* t_uncle2=t_node->m_parent->m_parent->m_left;
			if(((t_uncle2)!=0) && t_uncle2->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle2->m_color=1;
				t_uncle2->m_parent->m_color=-1;
				t_node=t_uncle2->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_left){
					t_node=t_node->m_parent;
					p_RotateRight(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateLeft(t_node->m_parent->m_parent);
			}
		}
	}
	m_root->m_color=1;
	return 0;
}
bool c_Map::p_Set(int t_key,c_DisplayMode* t_value){
	c_Node* t_node=m_root;
	c_Node* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=p_Compare(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				gc_assign(t_node->m_value,t_value);
				return false;
			}
		}
	}
	t_node=(new c_Node)->m_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->m_right,t_node);
		}else{
			gc_assign(t_parent->m_left,t_node);
		}
		p_InsertFixup(t_node);
	}else{
		gc_assign(m_root,t_node);
	}
	return true;
}
bool c_Map::p_Insert(int t_key,c_DisplayMode* t_value){
	return p_Set(t_key,t_value);
}
void c_Map::mark(){
	Object::mark();
	gc_mark_q(m_root);
}
c_IntMap::c_IntMap(){
}
c_IntMap* c_IntMap::m_new(){
	c_Map::m_new();
	return this;
}
int c_IntMap::p_Compare(int t_lhs,int t_rhs){
	return t_lhs-t_rhs;
}
void c_IntMap::mark(){
	c_Map::mark();
}
c_Stack::c_Stack(){
	m_data=Array<c_DisplayMode* >();
	m_length=0;
}
c_Stack* c_Stack::m_new(){
	return this;
}
c_Stack* c_Stack::m_new2(Array<c_DisplayMode* > t_data){
	gc_assign(this->m_data,t_data.Slice(0));
	this->m_length=t_data.Length();
	return this;
}
void c_Stack::p_Push(c_DisplayMode* t_value){
	if(m_length==m_data.Length()){
		gc_assign(m_data,m_data.Resize(m_length*2+10));
	}
	gc_assign(m_data[m_length],t_value);
	m_length+=1;
}
void c_Stack::p_Push2(Array<c_DisplayMode* > t_values,int t_offset,int t_count){
	for(int t_i=0;t_i<t_count;t_i=t_i+1){
		p_Push(t_values[t_offset+t_i]);
	}
}
void c_Stack::p_Push3(Array<c_DisplayMode* > t_values,int t_offset){
	p_Push2(t_values,t_offset,t_values.Length()-t_offset);
}
Array<c_DisplayMode* > c_Stack::p_ToArray(){
	Array<c_DisplayMode* > t_t=Array<c_DisplayMode* >(m_length);
	for(int t_i=0;t_i<m_length;t_i=t_i+1){
		gc_assign(t_t[t_i],m_data[t_i]);
	}
	return t_t;
}
void c_Stack::mark(){
	Object::mark();
	gc_mark_q(m_data);
}
c_Node::c_Node(){
	m_key=0;
	m_right=0;
	m_left=0;
	m_value=0;
	m_color=0;
	m_parent=0;
}
c_Node* c_Node::m_new(int t_key,c_DisplayMode* t_value,int t_color,c_Node* t_parent){
	this->m_key=t_key;
	gc_assign(this->m_value,t_value);
	this->m_color=t_color;
	gc_assign(this->m_parent,t_parent);
	return this;
}
c_Node* c_Node::m_new2(){
	return this;
}
void c_Node::mark(){
	Object::mark();
	gc_mark_q(m_right);
	gc_mark_q(m_left);
	gc_mark_q(m_value);
	gc_mark_q(m_parent);
}
Array<c_DisplayMode* > bb_app__displayModes;
c_DisplayMode* bb_app__desktopMode;
int bb_app_DeviceWidth(){
	return bb_app__devWidth;
}
int bb_app_DeviceHeight(){
	return bb_app__devHeight;
}
void bb_app_EnumDisplayModes(){
	Array<BBDisplayMode* > t_modes=bb_app__game->GetDisplayModes();
	c_IntMap* t_mmap=(new c_IntMap)->m_new();
	c_Stack* t_mstack=(new c_Stack)->m_new();
	for(int t_i=0;t_i<t_modes.Length();t_i=t_i+1){
		int t_w=t_modes[t_i]->width;
		int t_h=t_modes[t_i]->height;
		int t_size=t_w<<16|t_h;
		if(t_mmap->p_Contains(t_size)){
		}else{
			c_DisplayMode* t_mode=(new c_DisplayMode)->m_new(t_modes[t_i]->width,t_modes[t_i]->height);
			t_mmap->p_Insert(t_size,t_mode);
			t_mstack->p_Push(t_mode);
		}
	}
	gc_assign(bb_app__displayModes,t_mstack->p_ToArray());
	BBDisplayMode* t_mode2=bb_app__game->GetDesktopMode();
	if((t_mode2)!=0){
		gc_assign(bb_app__desktopMode,(new c_DisplayMode)->m_new(t_mode2->width,t_mode2->height));
	}else{
		gc_assign(bb_app__desktopMode,(new c_DisplayMode)->m_new(bb_app_DeviceWidth(),bb_app_DeviceHeight()));
	}
}
gxtkGraphics* bb_graphics_renderDevice;
int bb_graphics_SetMatrix(Float t_ix,Float t_iy,Float t_jx,Float t_jy,Float t_tx,Float t_ty){
	bb_graphics_context->m_ix=t_ix;
	bb_graphics_context->m_iy=t_iy;
	bb_graphics_context->m_jx=t_jx;
	bb_graphics_context->m_jy=t_jy;
	bb_graphics_context->m_tx=t_tx;
	bb_graphics_context->m_ty=t_ty;
	bb_graphics_context->m_tformed=((t_ix!=FLOAT(1.0) || t_iy!=FLOAT(0.0) || t_jx!=FLOAT(0.0) || t_jy!=FLOAT(1.0) || t_tx!=FLOAT(0.0) || t_ty!=FLOAT(0.0))?1:0);
	bb_graphics_context->m_matDirty=1;
	return 0;
}
int bb_graphics_SetMatrix2(Array<Float > t_m){
	bb_graphics_SetMatrix(t_m[0],t_m[1],t_m[2],t_m[3],t_m[4],t_m[5]);
	return 0;
}
int bb_graphics_SetColor(Float t_r,Float t_g,Float t_b){
	bb_graphics_context->m_color_r=t_r;
	bb_graphics_context->m_color_g=t_g;
	bb_graphics_context->m_color_b=t_b;
	bb_graphics_renderDevice->SetColor(t_r,t_g,t_b);
	return 0;
}
int bb_graphics_SetAlpha(Float t_alpha){
	bb_graphics_context->m_alpha=t_alpha;
	bb_graphics_renderDevice->SetAlpha(t_alpha);
	return 0;
}
int bb_graphics_SetBlend(int t_blend){
	bb_graphics_context->m_blend=t_blend;
	bb_graphics_renderDevice->SetBlend(t_blend);
	return 0;
}
int bb_graphics_SetScissor(Float t_x,Float t_y,Float t_width,Float t_height){
	bb_graphics_context->m_scissor_x=t_x;
	bb_graphics_context->m_scissor_y=t_y;
	bb_graphics_context->m_scissor_width=t_width;
	bb_graphics_context->m_scissor_height=t_height;
	bb_graphics_renderDevice->SetScissor(int(t_x),int(t_y),int(t_width),int(t_height));
	return 0;
}
int bb_graphics_BeginRender(){
	gc_assign(bb_graphics_renderDevice,bb_graphics_device);
	bb_graphics_context->m_matrixSp=0;
	bb_graphics_SetMatrix(FLOAT(1.0),FLOAT(0.0),FLOAT(0.0),FLOAT(1.0),FLOAT(0.0),FLOAT(0.0));
	bb_graphics_SetColor(FLOAT(255.0),FLOAT(255.0),FLOAT(255.0));
	bb_graphics_SetAlpha(FLOAT(1.0));
	bb_graphics_SetBlend(0);
	bb_graphics_SetScissor(FLOAT(0.0),FLOAT(0.0),Float(bb_app_DeviceWidth()),Float(bb_app_DeviceHeight()));
	return 0;
}
int bb_graphics_EndRender(){
	bb_graphics_renderDevice=0;
	return 0;
}
c_BBGameEvent::c_BBGameEvent(){
}
void c_BBGameEvent::mark(){
	Object::mark();
}
void bb_app_EndApp(){
	bbError(String());
}
c_VirtualDisplay::c_VirtualDisplay(){
	m_vwidth=FLOAT(.0);
	m_vheight=FLOAT(.0);
	m_vzoom=FLOAT(.0);
	m_lastvzoom=FLOAT(.0);
	m_vratio=FLOAT(.0);
	m_multi=FLOAT(.0);
	m_lastdevicewidth=0;
	m_lastdeviceheight=0;
	m_device_changed=0;
	m_fdw=FLOAT(.0);
	m_fdh=FLOAT(.0);
	m_dratio=FLOAT(.0);
	m_heightborder=FLOAT(.0);
	m_widthborder=FLOAT(.0);
	m_zoom_changed=0;
	m_realx=FLOAT(.0);
	m_realy=FLOAT(.0);
	m_offx=FLOAT(.0);
	m_offy=FLOAT(.0);
	m_sx=FLOAT(.0);
	m_sw=FLOAT(.0);
	m_sy=FLOAT(.0);
	m_sh=FLOAT(.0);
	m_scaledw=FLOAT(.0);
	m_scaledh=FLOAT(.0);
	m_vxoff=FLOAT(.0);
	m_vyoff=FLOAT(.0);
}
c_VirtualDisplay* c_VirtualDisplay::m_Display;
c_VirtualDisplay* c_VirtualDisplay::m_new(int t_width,int t_height,Float t_zoom){
	m_vwidth=Float(t_width);
	m_vheight=Float(t_height);
	m_vzoom=t_zoom;
	m_lastvzoom=m_vzoom+FLOAT(1.0);
	m_vratio=m_vheight/m_vwidth;
	gc_assign(m_Display,this);
	return this;
}
c_VirtualDisplay* c_VirtualDisplay::m_new2(){
	return this;
}
Float c_VirtualDisplay::p_VTouchX(int t_index,bool t_limit){
	Float t_touchoffset=bb_input_TouchX(t_index)-Float(bb_app_DeviceWidth())*FLOAT(0.5);
	Float t_x=t_touchoffset/m_multi/m_vzoom+bb_autofit_VDeviceWidth()*FLOAT(0.5);
	if(t_limit){
		Float t_widthlimit=m_vwidth-FLOAT(1.0);
		if(t_x>FLOAT(0.0)){
			if(t_x<t_widthlimit){
				return t_x;
			}else{
				return t_widthlimit;
			}
		}else{
			return FLOAT(0.0);
		}
	}else{
		return t_x;
	}
}
Float c_VirtualDisplay::p_VTouchY(int t_index,bool t_limit){
	Float t_touchoffset=bb_input_TouchY(t_index)-Float(bb_app_DeviceHeight())*FLOAT(0.5);
	Float t_y=t_touchoffset/m_multi/m_vzoom+bb_autofit_VDeviceHeight()*FLOAT(0.5);
	if(t_limit){
		Float t_heightlimit=m_vheight-FLOAT(1.0);
		if(t_y>FLOAT(0.0)){
			if(t_y<t_heightlimit){
				return t_y;
			}else{
				return t_heightlimit;
			}
		}else{
			return FLOAT(0.0);
		}
	}else{
		return t_y;
	}
}
int c_VirtualDisplay::p_UpdateVirtualDisplay(bool t_zoomborders,bool t_keepborders){
	if(bb_app_DeviceWidth()!=m_lastdevicewidth || bb_app_DeviceHeight()!=m_lastdeviceheight){
		m_lastdevicewidth=bb_app_DeviceWidth();
		m_lastdeviceheight=bb_app_DeviceHeight();
		m_device_changed=1;
	}
	if((m_device_changed)!=0){
		m_fdw=Float(bb_app_DeviceWidth());
		m_fdh=Float(bb_app_DeviceHeight());
		m_dratio=m_fdh/m_fdw;
		if(m_dratio>m_vratio){
			m_multi=m_fdw/m_vwidth;
			m_heightborder=(m_fdh-m_vheight*m_multi)*FLOAT(0.5);
			m_widthborder=FLOAT(0.0);
		}else{
			m_multi=m_fdh/m_vheight;
			m_widthborder=(m_fdw-m_vwidth*m_multi)*FLOAT(0.5);
			m_heightborder=FLOAT(0.0);
		}
	}
	if(m_vzoom!=m_lastvzoom){
		m_lastvzoom=m_vzoom;
		m_zoom_changed=1;
	}
	if(((m_zoom_changed)!=0) || ((m_device_changed)!=0)){
		if(t_zoomborders){
			m_realx=m_vwidth*m_vzoom*m_multi;
			m_realy=m_vheight*m_vzoom*m_multi;
			m_offx=(m_fdw-m_realx)*FLOAT(0.5);
			m_offy=(m_fdh-m_realy)*FLOAT(0.5);
			if(t_keepborders){
				if(m_offx<m_widthborder){
					m_sx=m_widthborder;
					m_sw=m_fdw-m_widthborder*FLOAT(2.0);
				}else{
					m_sx=m_offx;
					m_sw=m_fdw-m_offx*FLOAT(2.0);
				}
				if(m_offy<m_heightborder){
					m_sy=m_heightborder;
					m_sh=m_fdh-m_heightborder*FLOAT(2.0);
				}else{
					m_sy=m_offy;
					m_sh=m_fdh-m_offy*FLOAT(2.0);
				}
			}else{
				m_sx=m_offx;
				m_sw=m_fdw-m_offx*FLOAT(2.0);
				m_sy=m_offy;
				m_sh=m_fdh-m_offy*FLOAT(2.0);
			}
			m_sx=bb_math_Max2(FLOAT(0.0),m_sx);
			m_sy=bb_math_Max2(FLOAT(0.0),m_sy);
			m_sw=bb_math_Min2(m_sw,m_fdw);
			m_sh=bb_math_Min2(m_sh,m_fdh);
		}else{
			m_sx=bb_math_Max2(FLOAT(0.0),m_widthborder);
			m_sy=bb_math_Max2(FLOAT(0.0),m_heightborder);
			m_sw=bb_math_Min2(m_fdw-m_widthborder*FLOAT(2.0),m_fdw);
			m_sh=bb_math_Min2(m_fdh-m_heightborder*FLOAT(2.0),m_fdh);
		}
		m_scaledw=m_vwidth*m_multi*m_vzoom;
		m_scaledh=m_vheight*m_multi*m_vzoom;
		m_vxoff=(m_fdw-m_scaledw)*FLOAT(0.5);
		m_vyoff=(m_fdh-m_scaledh)*FLOAT(0.5);
		m_vxoff=m_vxoff/m_multi/m_vzoom;
		m_vyoff=m_vyoff/m_multi/m_vzoom;
		m_device_changed=0;
		m_zoom_changed=0;
	}
	bb_graphics_SetScissor(FLOAT(0.0),FLOAT(0.0),Float(bb_app_DeviceWidth()),Float(bb_app_DeviceHeight()));
	bb_graphics_Cls(FLOAT(199.0),FLOAT(179.0),FLOAT(156.0));
	bb_graphics_SetScissor(m_sx,m_sy,m_sw,m_sh);
	bb_graphics_Scale(m_multi*m_vzoom,m_multi*m_vzoom);
	if((m_vzoom)!=0){
		bb_graphics_Translate(m_vxoff,m_vyoff);
	}
	return 0;
}
void c_VirtualDisplay::mark(){
	Object::mark();
}
int bb_autofit_SetVirtualDisplay(int t_width,int t_height,Float t_zoom){
	(new c_VirtualDisplay)->m_new(t_width,t_height,t_zoom);
	return 0;
}
c_BitmapFont::c_BitmapFont(){
	m_borderChars=Array<c_BitMapChar* >();
	m_faceChars=Array<c_BitMapChar* >();
	m_shadowChars=Array<c_BitMapChar* >();
	m_packedImages=Array<c_Image* >();
	m__drawShadow=true;
	m__kerning=0;
	m__drawBorder=true;
}
int c_BitmapFont::p_LoadPacked(String t_info,String t_fontName,bool t_dynamicLoad){
	String t_header=t_info.Slice(0,t_info.Find(String(L",",1),0));
	String t_separator=String();
	String t_2=t_header;
	if(t_2==String(L"P1",2)){
		t_separator=String(L".",1);
	}else{
		if(t_2==String(L"P1.01",5)){
			t_separator=String(L"_P_",3);
		}
	}
	t_info=t_info.Slice(t_info.Find(String(L",",1),0)+1);
	gc_assign(m_borderChars,Array<c_BitMapChar* >(65536));
	gc_assign(m_faceChars,Array<c_BitMapChar* >(65536));
	gc_assign(m_shadowChars,Array<c_BitMapChar* >(65536));
	gc_assign(m_packedImages,Array<c_Image* >(256));
	int t_maxPacked=0;
	int t_maxChar=0;
	String t_prefixName=t_fontName;
	if(t_prefixName.ToLower().EndsWith(String(L".txt",4))){
		t_prefixName=t_prefixName.Slice(0,-4);
	}
	Array<String > t_charList=t_info.Split(String(L";",1));
	Array<String > t_=t_charList;
	int t_3=0;
	while(t_3<t_.Length()){
		String t_chr=t_[t_3];
		t_3=t_3+1;
		Array<String > t_chrdata=t_chr.Split(String(L",",1));
		if(t_chrdata.Length()<2){
			break;
		}
		c_BitMapChar* t_char=0;
		int t_charIndex=(t_chrdata[0]).ToInt();
		if(t_maxChar<t_charIndex){
			t_maxChar=t_charIndex;
		}
		String t_32=t_chrdata[1];
		if(t_32==String(L"B",1)){
			gc_assign(m_borderChars[t_charIndex],(new c_BitMapChar)->m_new());
			t_char=m_borderChars[t_charIndex];
		}else{
			if(t_32==String(L"F",1)){
				gc_assign(m_faceChars[t_charIndex],(new c_BitMapChar)->m_new());
				t_char=m_faceChars[t_charIndex];
			}else{
				if(t_32==String(L"S",1)){
					gc_assign(m_shadowChars[t_charIndex],(new c_BitMapChar)->m_new());
					t_char=m_shadowChars[t_charIndex];
				}
			}
		}
		t_char->m_packedFontIndex=(t_chrdata[2]).ToInt();
		if(m_packedImages[t_char->m_packedFontIndex]==0){
			gc_assign(m_packedImages[t_char->m_packedFontIndex],bb_graphics_LoadImage(t_prefixName+t_separator+String(t_char->m_packedFontIndex)+String(L".png",4),1,c_Image::m_DefaultFlags));
			if(t_maxPacked<t_char->m_packedFontIndex){
				t_maxPacked=t_char->m_packedFontIndex;
			}
		}
		t_char->m_packedPosition->m_x=Float((t_chrdata[3]).ToInt());
		t_char->m_packedPosition->m_y=Float((t_chrdata[4]).ToInt());
		t_char->m_packedSize->m_x=Float((t_chrdata[5]).ToInt());
		t_char->m_packedSize->m_y=Float((t_chrdata[6]).ToInt());
		t_char->m_drawingMetrics->m_drawingOffset->m_x=Float((t_chrdata[8]).ToInt());
		t_char->m_drawingMetrics->m_drawingOffset->m_y=Float((t_chrdata[9]).ToInt());
		t_char->m_drawingMetrics->m_drawingSize->m_x=Float((t_chrdata[10]).ToInt());
		t_char->m_drawingMetrics->m_drawingSize->m_y=Float((t_chrdata[11]).ToInt());
		t_char->m_drawingMetrics->m_drawingWidth=Float((t_chrdata[12]).ToInt());
	}
	gc_assign(m_borderChars,m_borderChars.Slice(0,t_maxChar+1));
	gc_assign(m_faceChars,m_faceChars.Slice(0,t_maxChar+1));
	gc_assign(m_shadowChars,m_shadowChars.Slice(0,t_maxChar+1));
	gc_assign(m_packedImages,m_packedImages.Slice(0,t_maxPacked+1));
	return 0;
}
int c_BitmapFont::p_LoadFontData(String t_Info,String t_fontName,bool t_dynamicLoad){
	if(t_Info.StartsWith(String(L"P1",2))){
		p_LoadPacked(t_Info,t_fontName,t_dynamicLoad);
		return 0;
	}
	Array<String > t_tokenStream=t_Info.Split(String(L",",1));
	int t_index=0;
	gc_assign(m_borderChars,Array<c_BitMapChar* >(65536));
	gc_assign(m_faceChars,Array<c_BitMapChar* >(65536));
	gc_assign(m_shadowChars,Array<c_BitMapChar* >(65536));
	String t_prefixName=t_fontName;
	if(t_prefixName.ToLower().EndsWith(String(L".txt",4))){
		t_prefixName=t_prefixName.Slice(0,-4);
	}
	int t_char=0;
	while(t_index<t_tokenStream.Length()){
		String t_strChar=t_tokenStream[t_index];
		if(t_strChar.Trim()==String()){
			t_index+=1;
			break;
		}
		t_char=(t_strChar).ToInt();
		t_index+=1;
		String t_kind=t_tokenStream[t_index];
		t_index+=1;
		String t_1=t_kind;
		if(t_1==String(L"{BR",3)){
			t_index+=3;
			gc_assign(m_borderChars[t_char],(new c_BitMapChar)->m_new());
			m_borderChars[t_char]->m_drawingMetrics->m_drawingOffset->m_x=Float((t_tokenStream[t_index]).ToInt());
			m_borderChars[t_char]->m_drawingMetrics->m_drawingOffset->m_y=Float((t_tokenStream[t_index+1]).ToInt());
			m_borderChars[t_char]->m_drawingMetrics->m_drawingSize->m_x=Float((t_tokenStream[t_index+2]).ToInt());
			m_borderChars[t_char]->m_drawingMetrics->m_drawingSize->m_y=Float((t_tokenStream[t_index+3]).ToInt());
			m_borderChars[t_char]->m_drawingMetrics->m_drawingWidth=Float((t_tokenStream[t_index+4]).ToInt());
			if(t_dynamicLoad==false){
				gc_assign(m_borderChars[t_char]->m_image,bb_graphics_LoadImage(t_prefixName+String(L"_BORDER_",8)+String(t_char)+String(L".png",4),1,c_Image::m_DefaultFlags));
				m_borderChars[t_char]->m_image->p_SetHandle(-m_borderChars[t_char]->m_drawingMetrics->m_drawingOffset->m_x,-m_borderChars[t_char]->m_drawingMetrics->m_drawingOffset->m_y);
			}else{
				m_borderChars[t_char]->p_SetImageResourceName(t_prefixName+String(L"_BORDER_",8)+String(t_char)+String(L".png",4));
			}
			t_index+=5;
			t_index+=1;
		}else{
			if(t_1==String(L"{SH",3)){
				t_index+=3;
				gc_assign(m_shadowChars[t_char],(new c_BitMapChar)->m_new());
				m_shadowChars[t_char]->m_drawingMetrics->m_drawingOffset->m_x=Float((t_tokenStream[t_index]).ToInt());
				m_shadowChars[t_char]->m_drawingMetrics->m_drawingOffset->m_y=Float((t_tokenStream[t_index+1]).ToInt());
				m_shadowChars[t_char]->m_drawingMetrics->m_drawingSize->m_x=Float((t_tokenStream[t_index+2]).ToInt());
				m_shadowChars[t_char]->m_drawingMetrics->m_drawingSize->m_y=Float((t_tokenStream[t_index+3]).ToInt());
				m_shadowChars[t_char]->m_drawingMetrics->m_drawingWidth=Float((t_tokenStream[t_index+4]).ToInt());
				String t_filename=t_prefixName+String(L"_SHADOW_",8)+String(t_char)+String(L".png",4);
				if(t_dynamicLoad==false){
					gc_assign(m_shadowChars[t_char]->m_image,bb_graphics_LoadImage(t_filename,1,c_Image::m_DefaultFlags));
					m_shadowChars[t_char]->m_image->p_SetHandle(-m_shadowChars[t_char]->m_drawingMetrics->m_drawingOffset->m_x,-m_shadowChars[t_char]->m_drawingMetrics->m_drawingOffset->m_y);
				}else{
					m_shadowChars[t_char]->p_SetImageResourceName(t_filename);
				}
				t_index+=5;
				t_index+=1;
			}else{
				if(t_1==String(L"{FC",3)){
					t_index+=3;
					gc_assign(m_faceChars[t_char],(new c_BitMapChar)->m_new());
					m_faceChars[t_char]->m_drawingMetrics->m_drawingOffset->m_x=Float((t_tokenStream[t_index]).ToInt());
					m_faceChars[t_char]->m_drawingMetrics->m_drawingOffset->m_y=Float((t_tokenStream[t_index+1]).ToInt());
					m_faceChars[t_char]->m_drawingMetrics->m_drawingSize->m_x=Float((t_tokenStream[t_index+2]).ToInt());
					m_faceChars[t_char]->m_drawingMetrics->m_drawingSize->m_y=Float((t_tokenStream[t_index+3]).ToInt());
					m_faceChars[t_char]->m_drawingMetrics->m_drawingWidth=Float((t_tokenStream[t_index+4]).ToInt());
					if(t_dynamicLoad==false){
						gc_assign(m_faceChars[t_char]->m_image,bb_graphics_LoadImage(t_prefixName+String(L"_",1)+String(t_char)+String(L".png",4),1,c_Image::m_DefaultFlags));
						m_faceChars[t_char]->m_image->p_SetHandle(-m_faceChars[t_char]->m_drawingMetrics->m_drawingOffset->m_x,-m_faceChars[t_char]->m_drawingMetrics->m_drawingOffset->m_y);
					}else{
						m_faceChars[t_char]->p_SetImageResourceName(t_prefixName+String(L"_",1)+String(t_char)+String(L".png",4));
					}
					t_index+=5;
					t_index+=1;
				}else{
					bbPrint(String(L"Error loading font! Char = ",27)+String(t_char));
				}
			}
		}
	}
	gc_assign(m_borderChars,m_borderChars.Slice(0,t_char+1));
	gc_assign(m_faceChars,m_faceChars.Slice(0,t_char+1));
	gc_assign(m_shadowChars,m_shadowChars.Slice(0,t_char+1));
	return 0;
}
c_BitmapFont* c_BitmapFont::m_new(String t_fontDescriptionFilePath,bool t_dynamicLoad){
	String t_text=bb_app_LoadString(t_fontDescriptionFilePath);
	if(t_text==String()){
		bbPrint(String(L"FONT ",5)+t_fontDescriptionFilePath+String(L" WAS NOT FOUND!!!",17));
	}
	p_LoadFontData(t_text,t_fontDescriptionFilePath,t_dynamicLoad);
	return this;
}
c_BitmapFont* c_BitmapFont::m_new2(String t_fontDescriptionFilePath){
	String t_text=bb_app_LoadString(t_fontDescriptionFilePath);
	if(t_text==String()){
		bbPrint(String(L"FONT ",5)+t_fontDescriptionFilePath+String(L" WAS NOT FOUND!!!",17));
	}
	p_LoadFontData(t_text,t_fontDescriptionFilePath,true);
	return this;
}
c_BitmapFont* c_BitmapFont::m_new3(){
	return this;
}
bool c_BitmapFont::p_DrawShadow(){
	return m__drawShadow;
}
int c_BitmapFont::p_DrawShadow2(bool t_value){
	m__drawShadow=t_value;
	return 0;
}
c_DrawingPoint* c_BitmapFont::p_Kerning(){
	if(m__kerning==0){
		gc_assign(m__kerning,(new c_DrawingPoint)->m_new2());
	}
	return m__kerning;
}
void c_BitmapFont::p_Kerning2(c_DrawingPoint* t_value){
	gc_assign(m__kerning,t_value);
}
Float c_BitmapFont::p_GetTxtWidth(String t_text,int t_fromChar,int t_toChar,bool t_endOnCR){
	Float t_twidth=FLOAT(.0);
	Float t_MaxWidth=FLOAT(0.0);
	int t_char=0;
	int t_lastchar=0;
	for(int t_i=t_fromChar;t_i<=t_toChar;t_i=t_i+1){
		t_char=(int)t_text[t_i-1];
		if(t_char>=0 && t_char<m_faceChars.Length() && t_char!=10 && t_char!=13){
			if(m_faceChars[t_char]!=0){
				t_lastchar=t_char;
				t_twidth=t_twidth+m_faceChars[t_char]->m_drawingMetrics->m_drawingWidth+p_Kerning()->m_x;
			}
		}else{
			if(t_char==10){
				if(bb_math_Abs2(t_MaxWidth)<bb_math_Abs2(t_twidth)){
					t_MaxWidth=t_twidth-p_Kerning()->m_x-m_faceChars[t_lastchar]->m_drawingMetrics->m_drawingWidth+m_faceChars[t_lastchar]->m_drawingMetrics->m_drawingSize->m_x;
				}
				t_twidth=FLOAT(0.0);
				t_lastchar=t_char;
				if(t_endOnCR){
					return t_MaxWidth;
				}
			}
		}
	}
	if(t_lastchar>=0 && t_lastchar<m_faceChars.Length()){
		if(t_lastchar==32){
		}else{
			if(m_faceChars[t_lastchar]!=0){
				t_twidth=t_twidth-m_faceChars[t_lastchar]->m_drawingMetrics->m_drawingWidth;
				t_twidth=t_twidth+m_faceChars[t_lastchar]->m_drawingMetrics->m_drawingSize->m_x;
			}
		}
	}
	if(bb_math_Abs2(t_MaxWidth)<bb_math_Abs2(t_twidth)){
		t_MaxWidth=t_twidth-p_Kerning()->m_x;
	}
	return t_MaxWidth;
}
Float c_BitmapFont::p_GetTxtWidth2(String t_text){
	return p_GetTxtWidth(t_text,1,t_text.Length(),false);
}
int c_BitmapFont::p_DrawCharsText(String t_text,Float t_x,Float t_y,Array<c_BitMapChar* > t_target,int t_align,int t_startPos,int t_endPos){
	Float t_drx=t_x;
	Float t_dry=t_y;
	Float t_oldX=t_x;
	int t_xOffset=0;
	if(t_endPos==-1 || t_endPos>t_text.Length()){
		t_endPos=t_text.Length();
	}
	if(t_align!=1){
		int t_lineSepPos=0;
		if(t_endPos!=-1){
			t_lineSepPos=t_endPos;
		}else{
			t_lineSepPos=t_text.Find(String(L"\n",1),t_startPos);
		}
		if(t_lineSepPos<0 || t_lineSepPos>t_endPos){
			t_lineSepPos=t_endPos;
		}
		int t_4=t_align;
		if(t_4==2){
			t_xOffset=int(this->p_GetTxtWidth(t_text,t_startPos,t_lineSepPos,true)/FLOAT(2.0));
		}else{
			if(t_4==3){
				t_xOffset=int(this->p_GetTxtWidth(t_text,t_startPos,t_lineSepPos,true));
			}
		}
	}
	for(int t_i=t_startPos;t_i<=t_endPos;t_i=t_i+1){
		int t_char=(int)t_text[t_i-1];
		if(t_char>=0 && t_char<=t_target.Length()){
			if(t_char==10){
				t_dry+=m_faceChars[32]->m_drawingMetrics->m_drawingSize->m_y+p_Kerning()->m_y;
				this->p_DrawCharsText(t_text,t_oldX,t_dry,t_target,t_align,t_i+1,t_endPos);
				return 0;
			}else{
				if(t_target[t_char]!=0){
					if(t_target[t_char]->p_CharImageLoaded()==false){
						t_target[t_char]->p_LoadCharImage();
					}
					if(t_target[t_char]->m_image!=0){
						bb_graphics_DrawImage(t_target[t_char]->m_image,t_drx-Float(t_xOffset),t_dry,0);
					}else{
						if(t_target[t_char]->m_packedFontIndex>0){
							bb_graphics_DrawImageRect(m_packedImages[t_target[t_char]->m_packedFontIndex],Float(-t_xOffset)+t_drx+t_target[t_char]->m_drawingMetrics->m_drawingOffset->m_x,t_dry+t_target[t_char]->m_drawingMetrics->m_drawingOffset->m_y,int(t_target[t_char]->m_packedPosition->m_x),int(t_target[t_char]->m_packedPosition->m_y),int(t_target[t_char]->m_packedSize->m_x),int(t_target[t_char]->m_packedSize->m_y),0);
						}
					}
					t_drx+=m_faceChars[t_char]->m_drawingMetrics->m_drawingWidth+p_Kerning()->m_x;
				}
			}
		}
	}
	return 0;
}
int c_BitmapFont::p_DrawCharsText2(String t_text,Float t_x,Float t_y,int t_mode,int t_align,int t_init,int t_ending){
	if(t_mode==1){
		p_DrawCharsText(t_text,t_x,t_y,m_borderChars,t_align,t_init,t_ending);
	}else{
		if(t_mode==0){
			p_DrawCharsText(t_text,t_x,t_y,m_faceChars,t_align,t_init,t_ending);
		}else{
			p_DrawCharsText(t_text,t_x,t_y,m_shadowChars,t_align,t_init,t_ending);
		}
	}
	return 0;
}
bool c_BitmapFont::p_DrawBorder(){
	return m__drawBorder;
}
int c_BitmapFont::p_DrawBorder2(bool t_value){
	m__drawBorder=t_value;
	return 0;
}
int c_BitmapFont::p_DrawText(String t_text,Float t_x,Float t_y,int t_align,int t_initChar,int t_endChar){
	if(p_DrawShadow()){
		p_DrawCharsText2(t_text,t_x,t_y,2,t_align,t_initChar,t_endChar);
	}
	if(p_DrawBorder()){
		p_DrawCharsText2(t_text,t_x,t_y,1,t_align,t_initChar,t_endChar);
	}
	p_DrawCharsText2(t_text,t_x,t_y,0,t_align,t_initChar,t_endChar);
	return 0;
}
int c_BitmapFont::p_DrawText2(String t_text,Float t_x,Float t_y){
	this->p_DrawText3(t_text,t_x,t_y,1);
	return 0;
}
int c_BitmapFont::p_DrawText3(String t_text,Float t_x,Float t_y,int t_align){
	p_DrawText(t_text,t_x,t_y,t_align,1,-1);
	return 0;
}
void c_BitmapFont::mark(){
	Object::mark();
	gc_mark_q(m_borderChars);
	gc_mark_q(m_faceChars);
	gc_mark_q(m_shadowChars);
	gc_mark_q(m_packedImages);
	gc_mark_q(m__kerning);
}
String bb_app_LoadString(String t_path){
	return bb_app__game->LoadString(bb_data_FixDataPath(t_path));
}
c_BitMapChar::c_BitMapChar(){
	m_packedFontIndex=0;
	m_packedPosition=(new c_DrawingPoint)->m_new2();
	m_packedSize=(new c_DrawingPoint)->m_new2();
	m_drawingMetrics=(new c_BitMapCharMetrics)->m_new();
	m_image=0;
	m_imageResourceName=String();
	m_imageResourceNameBackup=String();
}
c_BitMapChar* c_BitMapChar::m_new(){
	return this;
}
int c_BitMapChar::p_SetImageResourceName(String t_value){
	m_imageResourceName=t_value;
	return 0;
}
bool c_BitMapChar::p_CharImageLoaded(){
	if(m_image==0 && m_imageResourceName!=String()){
		return false;
	}else{
		return true;
	}
}
int c_BitMapChar::p_LoadCharImage(){
	if(p_CharImageLoaded()==false){
		gc_assign(m_image,bb_graphics_LoadImage(m_imageResourceName,1,c_Image::m_DefaultFlags));
		m_image->p_SetHandle(-this->m_drawingMetrics->m_drawingOffset->m_x,-this->m_drawingMetrics->m_drawingOffset->m_y);
		m_imageResourceNameBackup=m_imageResourceName;
		m_imageResourceName=String();
	}
	return 0;
}
void c_BitMapChar::mark(){
	Object::mark();
	gc_mark_q(m_packedPosition);
	gc_mark_q(m_packedSize);
	gc_mark_q(m_drawingMetrics);
	gc_mark_q(m_image);
}
c_DrawingPoint::c_DrawingPoint(){
	m_x=FLOAT(.0);
	m_y=FLOAT(.0);
}
c_DrawingPoint* c_DrawingPoint::m_new(Float t_x,Float t_y){
	this->m_x=t_x;
	this->m_y=t_y;
	return this;
}
c_DrawingPoint* c_DrawingPoint::m_new2(){
	return this;
}
void c_DrawingPoint::mark(){
	Object::mark();
}
c_BitMapCharMetrics::c_BitMapCharMetrics(){
	m_drawingOffset=(new c_DrawingPoint)->m_new2();
	m_drawingSize=(new c_DrawingPoint)->m_new2();
	m_drawingWidth=FLOAT(.0);
}
c_BitMapCharMetrics* c_BitMapCharMetrics::m_new(){
	return this;
}
void c_BitMapCharMetrics::mark(){
	Object::mark();
	gc_mark_q(m_drawingOffset);
	gc_mark_q(m_drawingSize);
}
c_Box2D_World::c_Box2D_World(){
	m_world=0;
	m_velocityIterations=0;
	m_positionIterations=0;
	m_timeStep=FLOAT(.0);
	m_scale=FLOAT(.0);
	m_entityList=0;
	m_debug=false;
	m_dbgDraw=(new c_b2DebugDraw)->m_new();
	m_debugVisible=false;
}
c_Box2D_World* c_Box2D_World::m_new(Float t_gravityX,Float t_gravityY,Float t_scale,bool t_debug){
	gc_assign(this->m_world,(new c_b2World)->m_new((new c_b2Vec2)->m_new(t_gravityX,t_gravityY),true));
	this->m_world->p_SetGravity((new c_b2Vec2)->m_new(t_gravityX,t_gravityY));
	this->m_velocityIterations=3;
	this->m_positionIterations=3;
	this->m_timeStep=FLOAT(0.016666666666666666);
	this->m_scale=t_scale;
	gc_assign(this->m_entityList,(new c_List)->m_new());
	this->m_debug=t_debug;
	if(this->m_debug==true){
		gc_assign(m_dbgDraw,(new c_b2DebugDraw)->m_new());
		m_dbgDraw->p_SetDrawScale(this->m_scale);
		m_dbgDraw->p_SetFillAlpha(FLOAT(0.3));
		m_dbgDraw->p_SetLineThickness(FLOAT(1.0));
		m_dbgDraw->p_SetFlags(c_b2DebugDraw::m_e_shapeBit|c_b2DebugDraw::m_e_jointBit);
		this->m_world->p_SetDebugDraw(m_dbgDraw);
		this->m_debugVisible=false;
	}
	return this;
}
int c_Box2D_World::p_KillEntity(c_Entity* t_e){
	this->m_world->p_DestroyBody(t_e->m_body);
	t_e->m_bodyShape=0;
	t_e->m_bodyDef=0;
	t_e->m_bodyShapeCircle=0;
	t_e->m_fixtureDef=0;
	t_e->m_img=0;
	t_e=0;
	return 0;
}
void c_Box2D_World::p_Clear(){
	c_Enumerator* t_=m_entityList->p_ObjectEnumerator();
	while(t_->p_HasNext()){
		c_Entity* t_e=t_->p_NextObject();
		p_KillEntity(t_e);
	}
}
c_Entity* c_Box2D_World::p_CreateMultiPolygon(Float t_x,Float t_y,c_List3* t_polygonList,bool t_static,Float t_friction){
	c_Entity* t_e=(new c_Entity)->m_new();
	t_e->p_CreateMultiPolygon2(this->m_world,t_x/this->m_scale,t_y/this->m_scale,t_polygonList,this->m_scale,t_static,t_friction);
	this->m_entityList->p_AddLast(t_e);
	return t_e;
}
c_Entity* c_Box2D_World::p_CreateImageBox(c_Image* t_img,Float t_x,Float t_y,bool t_static,Float t_restitution,Float t_density,Float t_friction,bool t_sensor){
	c_Entity* t_e=(new c_Entity)->m_new();
	t_e->p_CreateImageBox2(this->m_world,t_img,t_x/this->m_scale,t_y/this->m_scale,this->m_scale,t_restitution,t_density,t_friction,t_static,t_sensor);
	this->m_entityList->p_AddLast(t_e);
	return t_e;
}
void c_Box2D_World::p_Update(){
	this->m_world->p_TimeStep(this->m_timeStep,this->m_velocityIterations,this->m_positionIterations);
	this->m_world->p_ClearForces();
	if(this->m_debug==true){
		if(((bb_input_KeyHit(106))!=0) || ((bb_input_KeyHit(13))!=0)){
			if(this->m_debugVisible==false){
				this->m_debugVisible=true;
			}else{
				if(this->m_debugVisible==true){
					this->m_debugVisible=false;
				}
			}
		}
	}
}
void c_Box2D_World::p_Render(){
	if(this->m_entityList->p_Count()>0){
		c_Enumerator* t_=this->m_entityList->p_ObjectEnumerator();
		while(t_->p_HasNext()){
			c_Entity* t_e=t_->p_NextObject();
			if(t_e!=0){
				t_e->p_Draw2(FLOAT(0.0),FLOAT(0.0));
			}
		}
	}
	if(this->m_debugVisible==true){
		if(this->m_debug==true){
			m_world->p_DrawDebugData();
		}else{
			bbPrint(String(L"DEBUG MODE NOT SET!",19));
			this->m_debugVisible=false;
		}
	}
}
void c_Box2D_World::mark(){
	Object::mark();
	gc_mark_q(m_world);
	gc_mark_q(m_entityList);
	gc_mark_q(m_dbgDraw);
}
c_b2World::c_b2World(){
	m_m_destructionListener=0;
	m_m_debugDraw=0;
	m_m_bodyList=0;
	m_m_contactList=0;
	m_m_jointList=0;
	m_m_controllerList=0;
	m_m_bodyCount=0;
	m_m_contactCount=0;
	m_m_jointCount=0;
	m_m_controllerCount=0;
	m_m_allowSleep=false;
	m_m_gravity=0;
	m_m_inv_dt0=FLOAT(.0);
	m_m_contactManager=(new c_b2ContactManager)->m_new();
	m_m_flags=0;
	m_m_groundBody=0;
	m_m_island=(new c_b2Island)->m_new();
	m_m_contactSolver=(new c_b2ContactSolver)->m_new();
	m_stackCapacity=1000;
	m_s_stack=Array<c_b2Body* >(1000);
}
bool c_b2World::m_m_warmStarting;
bool c_b2World::m_m_continuousPhysics;
bool c_b2World::p_IsLocked(){
	return (m_m_flags&2)>0;
}
c_b2Body* c_b2World::p_CreateBody(c_b2BodyDef* t_def){
	if(p_IsLocked()==true){
		return 0;
	}
	c_b2Body* t_b=(new c_b2Body)->m_new(t_def,this);
	t_b->m_m_prev=0;
	gc_assign(t_b->m_m_next,m_m_bodyList);
	if((m_m_bodyList)!=0){
		gc_assign(m_m_bodyList->m_m_prev,t_b);
	}
	gc_assign(m_m_bodyList,t_b);
	m_m_bodyCount+=1;
	return t_b;
}
c_b2World* c_b2World::m_new(c_b2Vec2* t_gravity,bool t_doSleep){
	m_m_destructionListener=0;
	m_m_debugDraw=0;
	m_m_bodyList=0;
	m_m_contactList=0;
	m_m_jointList=0;
	m_m_controllerList=0;
	m_m_bodyCount=0;
	m_m_contactCount=0;
	m_m_jointCount=0;
	m_m_controllerCount=0;
	m_m_warmStarting=true;
	m_m_continuousPhysics=true;
	m_m_allowSleep=t_doSleep;
	gc_assign(m_m_gravity,t_gravity);
	m_m_inv_dt0=FLOAT(0.0);
	gc_assign(m_m_contactManager->m_m_world,this);
	c_b2BodyDef* t_bd=(new c_b2BodyDef)->m_new();
	gc_assign(m_m_groundBody,p_CreateBody(t_bd));
	return this;
}
c_b2World* c_b2World::m_new2(){
	return this;
}
void c_b2World::p_SetGravity(c_b2Vec2* t_gravity){
	gc_assign(m_m_gravity,t_gravity);
}
void c_b2World::p_SetDebugDraw(c_b2DebugDraw* t_debugDraw){
	gc_assign(m_m_debugDraw,t_debugDraw);
}
void c_b2World::p_DestroyJoint(c_b2Joint* t_j){
	bool t_collideConnected=t_j->m_m_collideConnected;
	if((t_j->m_m_prev)!=0){
		gc_assign(t_j->m_m_prev->m_m_next,t_j->m_m_next);
	}
	if((t_j->m_m_next)!=0){
		gc_assign(t_j->m_m_next->m_m_prev,t_j->m_m_prev);
	}
	if(t_j==m_m_jointList){
		gc_assign(m_m_jointList,t_j->m_m_next);
	}
	c_b2Body* t_bodyA=t_j->m_m_bodyA;
	c_b2Body* t_bodyB=t_j->m_m_bodyB;
	t_bodyA->p_SetAwake(true);
	t_bodyB->p_SetAwake(true);
	if((t_j->m_m_edgeA->m_prevItem)!=0){
		gc_assign(t_j->m_m_edgeA->m_prevItem->m_nextItem,t_j->m_m_edgeA->m_nextItem);
	}
	if((t_j->m_m_edgeA->m_nextItem)!=0){
		gc_assign(t_j->m_m_edgeA->m_nextItem->m_prevItem,t_j->m_m_edgeA->m_prevItem);
	}
	if(t_j->m_m_edgeA==t_bodyA->m_m_jointList){
		gc_assign(t_bodyA->m_m_jointList,t_j->m_m_edgeA->m_nextItem);
	}
	t_j->m_m_edgeA->m_prevItem=0;
	t_j->m_m_edgeA->m_nextItem=0;
	if((t_j->m_m_edgeB->m_prevItem)!=0){
		gc_assign(t_j->m_m_edgeB->m_prevItem->m_nextItem,t_j->m_m_edgeB->m_nextItem);
	}
	if((t_j->m_m_edgeB->m_nextItem)!=0){
		gc_assign(t_j->m_m_edgeB->m_nextItem->m_prevItem,t_j->m_m_edgeB->m_prevItem);
	}
	if(t_j->m_m_edgeB==t_bodyB->m_m_jointList){
		gc_assign(t_bodyB->m_m_jointList,t_j->m_m_edgeB->m_nextItem);
	}
	t_j->m_m_edgeB->m_prevItem=0;
	t_j->m_m_edgeB->m_nextItem=0;
	c_b2Joint::m_Destroy(t_j,0);
	m_m_jointCount-=1;
	if(t_collideConnected==false){
		c_b2ContactEdge* t_edge=t_bodyB->p_GetContactList();
		while((t_edge)!=0){
			if(t_edge->m_other==t_bodyA){
				t_edge->m_contact->p_FlagForFiltering();
			}
			t_edge=t_edge->m_nextItem;
		}
	}
}
void c_b2World::p_DestroyBody(c_b2Body* t_b){
	if(p_IsLocked()==true){
		return;
	}
	c_b2JointEdge* t_jn=t_b->m_m_jointList;
	while((t_jn)!=0){
		c_b2JointEdge* t_jn0=t_jn;
		t_jn=t_jn->m_nextItem;
		if((m_m_destructionListener)!=0){
			m_m_destructionListener->p_SayGoodbyeJoint(t_jn0->m_joint);
		}
		p_DestroyJoint(t_jn0->m_joint);
	}
	c_b2ControllerEdge* t_coe=t_b->m_m_controllerList;
	while((t_coe)!=0){
		c_b2ControllerEdge* t_coe0=t_coe;
		t_coe=t_coe->m_nextController;
		t_coe0->m_controller->p_RemoveBody(t_b);
	}
	c_b2ContactEdge* t_ce=t_b->m_m_contactList;
	while((t_ce)!=0){
		c_b2ContactEdge* t_ce0=t_ce;
		t_ce=t_ce->m_nextItem;
		m_m_contactManager->p_Destroy(t_ce0->m_contact);
	}
	t_b->m_m_contactList=0;
	c_b2Fixture* t_f=t_b->m_m_fixtureList;
	while((t_f)!=0){
		c_b2Fixture* t_f0=t_f;
		t_f=t_f->m_m_next;
		if((m_m_destructionListener)!=0){
			m_m_destructionListener->p_SayGoodbyeFixture(t_f0);
		}
		t_f0->p_DestroyProxy2(m_m_contactManager->m_m_broadPhase);
		t_f0->p_Destroy3();
	}
	t_b->m_m_fixtureList=0;
	t_b->m_m_fixtureCount=0;
	if((t_b->m_m_prev)!=0){
		gc_assign(t_b->m_m_prev->m_m_next,t_b->m_m_next);
	}
	if((t_b->m_m_next)!=0){
		gc_assign(t_b->m_m_next->m_m_prev,t_b->m_m_prev);
	}
	if(t_b==m_m_bodyList){
		gc_assign(m_m_bodyList,t_b->m_m_next);
	}
	m_m_bodyCount-=1;
}
void c_b2World::p_SetContactListener(c_b2ContactListenerInterface* t_listener){
	gc_assign(m_m_contactManager->m_m_contactListener,t_listener);
}
c_b2TimeStep* c_b2World::m_s_timestep2;
void c_b2World::p_Solve(c_b2TimeStep* t_timeStep){
	c_b2Body* t_b=0;
	c_b2Controller* t_controller=m_m_controllerList;
	while(t_controller!=0){
		t_controller->p_TimeStep2(t_timeStep);
		t_controller=t_controller->m_m_next;
	}
	c_b2Island* t_island=m_m_island;
	t_island->p_Initialize(m_m_bodyCount,m_m_contactCount,m_m_jointCount,0,m_m_contactManager->m_m_contactListener,m_m_contactSolver);
	t_b=m_m_bodyList;
	while(t_b!=0){
		t_b->m_m_flags&=-2;
		t_b=t_b->m_m_next;
	}
	c_b2Contact* t_c=m_m_contactList;
	while(t_c!=0){
		t_c->m_m_flags&=-5;
		t_c=t_c->m_m_next;
	}
	c_b2Joint* t_j=m_m_jointList;
	while(t_j!=0){
		t_j->m_m_islandFlag=false;
		t_j=t_j->m_m_next;
	}
	int t_stackSize=m_m_bodyCount;
	if(t_stackSize>m_stackCapacity){
		gc_assign(m_s_stack,m_s_stack.Resize(t_stackSize));
		m_stackCapacity=t_stackSize;
	}
	Array<c_b2Body* > t_stack=m_s_stack;
	c_b2Body* t_seed=m_m_bodyList;
	bool t_seedStart=true;
	while(t_seed!=0){
		if((t_seed->m_m_flags&1)!=0){
			t_seed=t_seed->m_m_next;
			continue;
		}
		if((t_seed->m_m_flags&34)!=34){
			t_seed=t_seed->m_m_next;
			continue;
		}
		if(t_seed->m_m_type==0){
			t_seed=t_seed->m_m_next;
			continue;
		}
		t_island->p_Clear();
		int t_stackCount=0;
		gc_assign(t_stack[t_stackCount],t_seed);
		t_stackCount+=1;
		t_seed->m_m_flags|=1;
		while(t_stackCount>0){
			t_stackCount-=1;
			t_b=t_stack[t_stackCount];
			t_island->p_AddBody(t_b);
			if(!((t_b->m_m_flags&2)!=0)){
				t_b->p_SetAwake(true);
			}
			if(t_b->m_m_type==0){
				continue;
			}
			c_b2Body* t_other=0;
			c_b2ContactEdge* t_ce=t_b->m_m_contactList;
			while(t_ce!=0){
				if((t_ce->m_contact->m_m_flags&4)!=0){
					t_ce=t_ce->m_nextItem;
					continue;
				}
				if((t_ce->m_contact->m_m_flags&48)!=48 || ((t_ce->m_contact->m_m_flags&1)!=0)){
					t_ce=t_ce->m_nextItem;
					continue;
				}
				t_island->p_AddContact(t_ce->m_contact);
				t_ce->m_contact->m_m_flags|=4;
				t_other=t_ce->m_other;
				if((t_other->m_m_flags&1)!=0){
					t_ce=t_ce->m_nextItem;
					continue;
				}
				gc_assign(t_stack[t_stackCount],t_other);
				t_stackCount+=1;
				t_other->m_m_flags|=1;
				t_ce=t_ce->m_nextItem;
			}
			c_b2JointEdge* t_jn=t_b->m_m_jointList;
			while(t_jn!=0){
				if(t_jn->m_joint->m_m_islandFlag==true){
					t_jn=t_jn->m_nextItem;
					continue;
				}
				t_other=t_jn->m_other;
				if(!((t_other->m_m_flags&32)!=0)){
					t_jn=t_jn->m_nextItem;
					continue;
				}
				t_island->p_AddJoint(t_jn->m_joint);
				t_jn->m_joint->m_m_islandFlag=true;
				if((t_other->m_m_flags&1)!=0){
					t_jn=t_jn->m_nextItem;
					continue;
				}
				gc_assign(t_stack[t_stackCount],t_other);
				t_stackCount+=1;
				t_other->m_m_flags|=1;
				t_jn=t_jn->m_nextItem;
			}
		}
		t_island->p_Solve4(t_timeStep,m_m_gravity,m_m_allowSleep);
		for(int t_i=0;t_i<t_island->m_m_bodyCount;t_i=t_i+1){
			t_b=t_island->m_m_bodies[t_i];
			if(t_b->m_m_type==0){
				t_b->m_m_flags&=-2;
			}
		}
		t_seed=t_seed->m_m_next;
	}
	for(int t_i2=0;t_i2<t_stack.Length();t_i2=t_i2+1){
		if(t_stack[t_i2]==0){
			break;
		}
		t_stack[t_i2]=0;
	}
	t_b=m_m_bodyList;
	while(t_b!=0){
		if((t_b->m_m_flags&34)!=34){
			t_b=t_b->m_m_next;
			continue;
		}
		if(t_b->m_m_type==0){
			t_b=t_b->m_m_next;
			continue;
		}
		t_b->p_SynchronizeFixtures();
		t_b=t_b->m_m_next;
	}
	m_m_contactManager->p_FindNewContacts();
}
Array<c_b2Body* > c_b2World::m_s_queue;
c_b2Sweep* c_b2World::m_s_backupA;
c_b2Sweep* c_b2World::m_s_backupB;
c_b2TimeStep* c_b2World::m_s_timestep;
void c_b2World::p_SolveTOI(c_b2TimeStep* t_timeStep){
	c_b2Body* t_b=0;
	c_b2Fixture* t_fA=0;
	c_b2Fixture* t_fB=0;
	c_b2Body* t_bA=0;
	c_b2Body* t_bB=0;
	c_b2ContactEdge* t_cEdge=0;
	c_b2Joint* t_j=0;
	c_b2Island* t_island=m_m_island;
	t_island->p_Initialize(m_m_bodyCount,32,32,0,m_m_contactManager->m_m_contactListener,m_m_contactSolver);
	if(m_m_bodyCount>m_s_queue.Length()){
		gc_assign(m_s_queue,m_s_queue.Resize(m_m_bodyCount));
	}
	Array<c_b2Body* > t_queue=m_s_queue;
	t_b=m_m_bodyList;
	while(t_b!=0){
		t_b->m_m_flags&=-2;
		t_b->m_m_sweep->m_t0=FLOAT(0.0);
		t_b=t_b->m_m_next;
	}
	c_b2Contact* t_c=0;
	t_c=m_m_contactList;
	while(t_c!=0){
		t_c->m_m_flags&=-13;
		t_c=t_c->m_m_next;
	}
	t_j=m_m_jointList;
	while(t_j!=0){
		t_j->m_m_islandFlag=false;
		t_j=t_j->m_m_next;
	}
	while(true){
		c_b2Contact* t_minContact=0;
		Float t_minTOI=FLOAT(1.0);
		t_c=m_m_contactList;
		while(t_c!=0){
			if((t_c->m_m_flags&34)!=34 || ((t_c->m_m_flags&1)!=0)){
				t_c=t_c->m_m_next;
				continue;
			}
			Float t_toi=FLOAT(1.0);
			if((t_c->m_m_flags&8)!=0){
				t_toi=t_c->m_m_toi;
			}else{
				t_fA=t_c->m_m_fixtureA;
				t_fB=t_c->m_m_fixtureB;
				t_bA=t_fA->m_m_body;
				t_bB=t_fB->m_m_body;
				if((t_bA->m_m_type!=2 || !((t_bA->m_m_flags&2)!=0)) && (t_bB->m_m_type!=2 || !((t_bA->m_m_flags&2)!=0))){
					t_c=t_c->m_m_next;
					continue;
				}
				Float t_t0=t_bA->m_m_sweep->m_t0;
				if(t_bA->m_m_sweep->m_t0<t_bB->m_m_sweep->m_t0){
					t_t0=t_bB->m_m_sweep->m_t0;
					t_bA->m_m_sweep->p_Advance(t_t0);
				}else{
					if(t_bB->m_m_sweep->m_t0<t_bA->m_m_sweep->m_t0){
						t_t0=t_bA->m_m_sweep->m_t0;
						t_bB->m_m_sweep->p_Advance(t_t0);
					}
				}
				t_toi=t_c->p_ComputeTOI(t_bA->m_m_sweep,t_bB->m_m_sweep);
				if(t_toi>FLOAT(0.0) && t_toi<FLOAT(1.0)){
					t_toi=(FLOAT(1.0)-t_toi)*t_t0+t_toi;
					if(t_toi>FLOAT(1.0)){
						t_toi=FLOAT(1.0);
					}
				}
				t_c->m_m_toi=t_toi;
				t_c->m_m_flags|=8;
			}
			if(FLOAT(1e-15)<t_toi && t_toi<t_minTOI){
				t_minContact=t_c;
				t_minTOI=t_toi;
			}
			t_c=t_c->m_m_next;
		}
		if(t_minContact==0 || FLOAT(0.99999999999989997)<t_minTOI){
			break;
		}
		t_fA=t_minContact->m_m_fixtureA;
		t_fB=t_minContact->m_m_fixtureB;
		t_bA=t_fA->m_m_body;
		t_bB=t_fB->m_m_body;
		m_s_backupA->p_Set7(t_bA->m_m_sweep);
		m_s_backupB->p_Set7(t_bB->m_m_sweep);
		t_bA->p_Advance(t_minTOI);
		t_bB->p_Advance(t_minTOI);
		t_minContact->p_Update2(m_m_contactManager->m_m_contactListener);
		t_minContact->m_m_flags&=-9;
		if(((t_minContact->m_m_flags&1)!=0) || !((t_minContact->m_m_flags&32)!=0)){
			t_bA->m_m_sweep->p_Set7(m_s_backupA);
			t_bB->m_m_sweep->p_Set7(m_s_backupB);
			t_bA->p_SynchronizeTransform();
			t_bB->p_SynchronizeTransform();
			continue;
		}
		if(!((t_minContact->m_m_flags&16)!=0)){
			continue;
		}
		c_b2Body* t_seed=t_bA;
		if(t_seed->m_m_type!=2){
			t_seed=t_bB;
		}
		t_island->p_Clear();
		c_b2Body* t_other=0;
		int t_queueStart=0;
		int t_queueSize=0;
		gc_assign(t_queue[t_queueStart+t_queueSize],t_seed);
		t_queueSize+=1;
		t_seed->m_m_flags|=1;
		while(t_queueSize>0){
			t_b=t_queue[t_queueStart];
			t_queueStart+=1;
			t_queueSize-=1;
			t_island->p_AddBody(t_b);
			if(!((t_b->m_m_flags&2)!=0)){
				t_b->p_SetAwake(true);
			}
			if(t_b->m_m_type!=2){
				continue;
			}
			t_cEdge=t_b->m_m_contactList;
			while(t_cEdge!=0){
				if(t_island->m_m_contactCount==t_island->m_m_contactCapacity){
					break;
				}
				if((t_cEdge->m_contact->m_m_flags&4)!=0){
					t_cEdge=t_cEdge->m_nextItem;
					continue;
				}
				if((t_cEdge->m_contact->m_m_flags&48)!=48 || ((t_cEdge->m_contact->m_m_flags&1)!=0)){
					t_cEdge=t_cEdge->m_nextItem;
					continue;
				}
				t_island->p_AddContact(t_cEdge->m_contact);
				t_cEdge->m_contact->m_m_flags|=4;
				t_other=t_cEdge->m_other;
				if((t_other->m_m_flags&1)!=0){
					t_cEdge=t_cEdge->m_nextItem;
					continue;
				}
				if(t_other->m_m_type!=0){
					t_other->p_Advance(t_minTOI);
					t_other->p_SetAwake(true);
				}
				gc_assign(t_queue[t_queueStart+t_queueSize],t_other);
				t_queueSize+=1;
				t_other->m_m_flags|=1;
				t_cEdge=t_cEdge->m_nextItem;
			}
			c_b2JointEdge* t_jEdge=t_b->m_m_jointList;
			while(t_jEdge!=0){
				if(t_island->m_m_jointCount==t_island->m_m_jointCapacity){
					t_jEdge=t_jEdge->m_nextItem;
					continue;
				}
				if(t_jEdge->m_joint->m_m_islandFlag==true){
					t_jEdge=t_jEdge->m_nextItem;
					continue;
				}
				t_other=t_jEdge->m_other;
				if(t_other->p_IsActive()==false){
					t_jEdge=t_jEdge->m_nextItem;
					continue;
				}
				t_island->p_AddJoint(t_jEdge->m_joint);
				t_jEdge->m_joint->m_m_islandFlag=true;
				if((t_other->m_m_flags&1)!=0){
					t_jEdge=t_jEdge->m_nextItem;
					continue;
				}
				if(t_other->m_m_type!=0){
					t_other->p_Advance(t_minTOI);
					t_other->p_SetAwake(true);
				}
				gc_assign(t_queue[t_queueStart+t_queueSize],t_other);
				t_queueSize+=1;
				t_other->m_m_flags|=1;
				t_jEdge=t_jEdge->m_nextItem;
			}
		}
		c_b2TimeStep* t_subStep=m_s_timestep;
		t_subStep->m_warmStarting=false;
		t_subStep->m_dt=(FLOAT(1.0)-t_minTOI)*t_timeStep->m_dt;
		t_subStep->m_inv_dt=FLOAT(1.0)/t_subStep->m_dt;
		t_subStep->m_dtRatio=FLOAT(0.0);
		t_subStep->m_velocityIterations=t_timeStep->m_velocityIterations;
		t_subStep->m_positionIterations=t_timeStep->m_positionIterations;
		t_island->p_SolveTOI(t_subStep);
		for(int t_i=0;t_i<t_island->m_m_bodyCount;t_i=t_i+1){
			t_b=t_island->m_m_bodies[t_i];
			t_b->m_m_flags&=-2;
			if(!((t_b->m_m_flags&2)!=0)){
				continue;
			}
			if(t_b->m_m_type!=2){
				continue;
			}
			t_b->p_SynchronizeFixtures();
			t_cEdge=t_b->m_m_contactList;
			while(t_cEdge!=0){
				t_cEdge->m_contact->m_m_flags&=-9;
				t_cEdge=t_cEdge->m_nextItem;
			}
		}
		for(int t_i2=0;t_i2<t_island->m_m_contactCount;t_i2=t_i2+1){
			t_c=t_island->m_m_contacts[t_i2];
			t_c->m_m_flags&=-13;
		}
		for(int t_i3=0;t_i3<t_island->m_m_jointCount;t_i3=t_i3+1){
			t_j=t_island->m_m_joints[t_i3];
			t_j->m_m_islandFlag=false;
		}
		m_m_contactManager->p_FindNewContacts();
	}
}
void c_b2World::p_TimeStep(Float t_dt,int t_velocityIterations,int t_positionIterations){
	if((m_m_flags&1)!=0){
		m_m_contactManager->p_FindNewContacts();
		m_m_flags&=-2;
	}
	m_m_flags|=2;
	c_b2TimeStep* t_timeStep=m_s_timestep2;
	t_timeStep->m_dt=t_dt;
	t_timeStep->m_velocityIterations=t_velocityIterations;
	t_timeStep->m_positionIterations=t_positionIterations;
	if(t_dt>FLOAT(0.0)){
		t_timeStep->m_inv_dt=FLOAT(1.0)/t_dt;
	}else{
		t_timeStep->m_inv_dt=FLOAT(0.0);
	}
	t_timeStep->m_dtRatio=m_m_inv_dt0*t_dt;
	t_timeStep->m_warmStarting=m_m_warmStarting;
	m_m_contactManager->p_Collide();
	if(t_timeStep->m_dt>FLOAT(0.0)){
		p_Solve(t_timeStep);
	}
	if(m_m_continuousPhysics && t_timeStep->m_dt>FLOAT(0.0)){
		p_SolveTOI(t_timeStep);
	}
	if(t_timeStep->m_dt>FLOAT(0.0)){
		m_m_inv_dt0=t_timeStep->m_inv_dt;
	}
	m_m_flags&=-3;
}
void c_b2World::p_ClearForces(){
	c_b2Body* t_body=m_m_bodyList;
	while(t_body!=0){
		t_body->m_m_force->p_SetZero();
		t_body->m_m_torque=FLOAT(0.0);
		t_body=t_body->m_m_next;
	}
}
void c_b2World::p_DrawShape(c_b2Shape* t_shape,c_b2Transform* t_xf,c_b2Color* t_color){
	int t_2=t_shape->m_m_type;
	if(t_2==0){
		c_b2CircleShape* t_circle=dynamic_cast<c_b2CircleShape*>(t_shape);
		c_b2Vec2* t_center=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
		c_b2Math::m_MulX(t_xf,t_circle->m_m_p,t_center);
		Float t_radius=t_circle->m_m_radius;
		c_b2Vec2* t_axis=t_xf->m_R->m_col1;
		m_m_debugDraw->p_DrawSolidCircle(t_center,t_radius,t_axis,t_color);
	}else{
		if(t_2==1){
			int t_i=0;
			c_b2PolygonShape* t_poly=dynamic_cast<c_b2PolygonShape*>(t_shape);
			int t_vertexCount=t_poly->p_GetVertexCount();
			Array<c_b2Vec2* > t_localVertices=t_poly->p_GetVertices();
			Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >(t_vertexCount);
			for(int t_i2=0;t_i2<t_vertexCount;t_i2=t_i2+1){
				gc_assign(t_vertices[t_i2],(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0)));
				c_b2Math::m_MulX(t_xf,t_localVertices[t_i2],t_vertices[t_i2]);
			}
			m_m_debugDraw->p_DrawSolidPolygon(t_vertices,t_vertexCount,t_color);
		}else{
			if(t_2==2){
				c_b2EdgeShape* t_edge=dynamic_cast<c_b2EdgeShape*>(t_shape);
				c_b2Vec2* t_e1=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
				c_b2Vec2* t_e2=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
				c_b2Math::m_MulX(t_xf,t_edge->p_GetVertex1(),t_e1);
				c_b2Math::m_MulX(t_xf,t_edge->p_GetVertex2(),t_e2);
				m_m_debugDraw->p_DrawSegment(t_e1,t_e2,t_color);
			}
		}
	}
}
c_b2Color* c_b2World::m_s_jointColor;
void c_b2World::p_DrawJoint(c_b2Joint* t_joint){
	c_b2Body* t_b1=t_joint->p_GetBodyA();
	c_b2Body* t_b2=t_joint->p_GetBodyB();
	c_b2Transform* t_xf1=t_b1->m_m_xf;
	c_b2Transform* t_xf2=t_b2->m_m_xf;
	c_b2Vec2* t_x1=t_xf1->m_position;
	c_b2Vec2* t_x2=t_xf2->m_position;
	c_b2Vec2* t_p1=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	t_joint->p_GetAnchorA(t_p1);
	c_b2Vec2* t_p2=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	t_joint->p_GetAnchorB(t_p2);
	c_b2Color* t_color=m_s_jointColor;
	int t_1=t_joint->m_m_type;
	if(t_1==3){
		m_m_debugDraw->p_DrawSegment(t_p1,t_p2,t_color);
	}else{
		if(t_1==4){
			c_b2PulleyJoint* t_pulley=dynamic_cast<c_b2PulleyJoint*>(t_joint);
			c_b2Vec2* t_s1=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
			t_pulley->p_GetGroundAnchorA(t_s1);
			c_b2Vec2* t_s2=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
			t_pulley->p_GetGroundAnchorB(t_s2);
			m_m_debugDraw->p_DrawSegment(t_s1,t_p1,t_color);
			m_m_debugDraw->p_DrawSegment(t_s2,t_p2,t_color);
			m_m_debugDraw->p_DrawSegment(t_s1,t_s2,t_color);
		}else{
			if(t_1==5){
				m_m_debugDraw->p_DrawSegment(t_p1,t_p2,t_color);
			}else{
				if(t_b1!=m_m_groundBody){
					m_m_debugDraw->p_DrawSegment(t_x1,t_p1,t_color);
				}
				m_m_debugDraw->p_DrawSegment(t_p1,t_p2,t_color);
				if(t_b2!=m_m_groundBody){
					m_m_debugDraw->p_DrawSegment(t_x2,t_p2,t_color);
				}
			}
		}
	}
}
c_b2Transform* c_b2World::m_s_xf;
void c_b2World::p_DrawDebugData(){
	if(m_m_debugDraw==0){
		return;
	}
	m_m_debugDraw->p_Clear();
	int t_flags=m_m_debugDraw->p_GetFlags();
	int t_i=0;
	c_b2Body* t_b=0;
	c_b2Fixture* t_f=0;
	c_b2Shape* t_s=0;
	c_b2Joint* t_j=0;
	c_IBroadPhase* t_bp=0;
	c_b2Vec2* t_invQ=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	c_b2Vec2* t_x1=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	c_b2Vec2* t_x2=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	c_b2Transform* t_xf=0;
	c_b2AABB* t_b1=(new c_b2AABB)->m_new();
	c_b2AABB* t_b2=(new c_b2AABB)->m_new();
	c_b2Vec2* t_[]={(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0)),(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0)),(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0)),(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0))};
	Array<c_b2Vec2* > t_vs=Array<c_b2Vec2* >(t_,4);
	c_b2Color* t_color=(new c_b2Color)->m_new(FLOAT(0.0),FLOAT(0.0),FLOAT(0.0));
	if((t_flags&c_b2DebugDraw::m_e_shapeBit)!=0){
		t_b=m_m_bodyList;
		while(t_b!=0){
			t_xf=t_b->m_m_xf;
			t_f=t_b->p_GetFixtureList();
			while(t_f!=0){
				t_s=t_f->p_GetShape();
				if(t_b->p_IsActive()==false){
					t_color->p_Set11(FLOAT(0.5),FLOAT(0.5),FLOAT(0.3));
					p_DrawShape(t_s,t_xf,t_color);
				}else{
					if(t_b->p_GetType()==0){
						t_color->p_Set11(FLOAT(0.5),FLOAT(0.9),FLOAT(0.5));
						p_DrawShape(t_s,t_xf,t_color);
					}else{
						if(t_b->p_GetType()==1){
							t_color->p_Set11(FLOAT(0.5),FLOAT(0.5),FLOAT(0.9));
							p_DrawShape(t_s,t_xf,t_color);
						}else{
							if(t_b->p_IsAwake()==false){
								t_color->p_Set11(FLOAT(0.6),FLOAT(0.6),FLOAT(0.6));
								p_DrawShape(t_s,t_xf,t_color);
							}else{
								t_color->p_Set11(FLOAT(0.9),FLOAT(0.7),FLOAT(0.7));
								p_DrawShape(t_s,t_xf,t_color);
							}
						}
					}
				}
				t_f=t_f->m_m_next;
			}
			t_b=t_b->m_m_next;
		}
	}
	if((t_flags&c_b2DebugDraw::m_e_jointBit)!=0){
		t_j=m_m_jointList;
		while(t_j!=0){
			p_DrawJoint(t_j);
			t_j=t_j->m_m_next;
		}
	}
	if((t_flags&c_b2DebugDraw::m_e_controllerBit)!=0){
		c_b2Controller* t_c=m_m_controllerList;
		while(t_c!=0){
			t_c->p_Draw(m_m_debugDraw);
			t_c=t_c->m_m_next;
		}
	}
	if((t_flags&c_b2DebugDraw::m_e_pairBit)!=0){
		t_color->p_Set11(FLOAT(0.3),FLOAT(0.9),FLOAT(0.9));
		c_b2Contact* t_contact=m_m_contactList;
		while(t_contact!=0){
			if((dynamic_cast<c_b2PolyAndCircleContact*>(t_contact))!=0){
				c_b2Fixture* t_fixtureA=t_contact->p_GetFixtureA();
				c_b2Fixture* t_fixtureB=t_contact->p_GetFixtureB();
				c_b2Vec2* t_cA=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
				t_fixtureA->p_GetAABB()->p_GetCenter(t_cA);
				c_b2Vec2* t_cB=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
				t_fixtureB->p_GetAABB()->p_GetCenter(t_cB);
				m_m_debugDraw->p_DrawSegment(t_cA,t_cB,t_color);
			}
			t_contact=t_contact->p_GetNext();
		}
	}
	if((t_flags&c_b2DebugDraw::m_e_aabbBit)!=0){
		t_bp=m_m_contactManager->m_m_broadPhase;
		c_b2Vec2* t_2[]={(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0)),(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0)),(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0)),(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0))};
		t_vs=Array<c_b2Vec2* >(t_2,4);
		t_b=m_m_bodyList;
		while(t_b!=0){
			if(t_b->p_IsActive()==false){
				t_b=t_b->p_GetNext();
				continue;
			}
			t_f=t_b->p_GetFixtureList();
			while(t_f!=0){
				c_b2AABB* t_aabb=t_bp->p_GetFatAABB(t_f->m_m_proxy);
				t_vs[0]->p_Set2(t_aabb->m_lowerBound->m_x,t_aabb->m_lowerBound->m_y);
				t_vs[1]->p_Set2(t_aabb->m_upperBound->m_x,t_aabb->m_lowerBound->m_y);
				t_vs[2]->p_Set2(t_aabb->m_upperBound->m_x,t_aabb->m_upperBound->m_y);
				t_vs[3]->p_Set2(t_aabb->m_lowerBound->m_x,t_aabb->m_upperBound->m_y);
				m_m_debugDraw->p_DrawPolygon(t_vs,4,t_color);
				t_f=t_f->p_GetNext();
			}
			t_b=t_b->p_GetNext();
		}
	}
	if((t_flags&c_b2DebugDraw::m_e_centerOfMassBit)!=0){
		t_b=m_m_bodyList;
		while(t_b!=0){
			t_xf=m_s_xf;
			gc_assign(t_xf->m_R,t_b->m_m_xf->m_R);
			gc_assign(t_xf->m_position,t_b->p_GetWorldCenter());
			m_m_debugDraw->p_DrawTransform(t_xf);
			t_b=t_b->m_m_next;
		}
	}
}
void c_b2World::mark(){
	Object::mark();
	gc_mark_q(m_m_destructionListener);
	gc_mark_q(m_m_debugDraw);
	gc_mark_q(m_m_bodyList);
	gc_mark_q(m_m_contactList);
	gc_mark_q(m_m_jointList);
	gc_mark_q(m_m_controllerList);
	gc_mark_q(m_m_gravity);
	gc_mark_q(m_m_contactManager);
	gc_mark_q(m_m_groundBody);
	gc_mark_q(m_m_island);
	gc_mark_q(m_m_contactSolver);
	gc_mark_q(m_s_stack);
}
c_b2Vec2::c_b2Vec2(){
	m_x=FLOAT(.0);
	m_y=FLOAT(.0);
}
c_b2Vec2* c_b2Vec2::m_new(Float t_x_,Float t_y_){
	m_x=t_x_;
	m_y=t_y_;
	return this;
}
void c_b2Vec2::p_Set2(Float t_x_,Float t_y_){
	m_x=t_x_;
	m_y=t_y_;
}
void c_b2Vec2::p_SetV(c_b2Vec2* t_v){
	m_x=t_v->m_x;
	m_y=t_v->m_y;
}
void c_b2Vec2::p_SetZero(){
	m_x=FLOAT(0.0);
	m_y=FLOAT(0.0);
}
Float c_b2Vec2::p_LengthSquared(){
	return m_x*m_x+m_y*m_y;
}
Float c_b2Vec2::p_Normalize(){
	Float t_length=(Float)sqrt(m_x*m_x+m_y*m_y);
	if(t_length<FLOAT(1e-15)){
		return FLOAT(0.0);
	}
	Float t_invLength=FLOAT(1.0)/t_length;
	m_x*=t_invLength;
	m_y*=t_invLength;
	return t_length;
}
c_b2Vec2* c_b2Vec2::m_Make(Float t_x_,Float t_y_){
	return (new c_b2Vec2)->m_new(t_x_,t_y_);
}
c_b2Vec2* c_b2Vec2::p_Copy(){
	return (new c_b2Vec2)->m_new(m_x,m_y);
}
Float c_b2Vec2::p_Length(){
	return (Float)sqrt(m_x*m_x+m_y*m_y);
}
void c_b2Vec2::p_GetNegative(c_b2Vec2* t_out){
	t_out->p_Set2(-m_x,-m_y);
}
void c_b2Vec2::p_Multiply(Float t_a){
	m_x*=t_a;
	m_y*=t_a;
}
void c_b2Vec2::p_NegativeSelf(){
	m_x=-m_x;
	m_y=-m_y;
}
void c_b2Vec2::p_Add(c_b2Vec2* t_v){
	m_x+=t_v->m_x;
	m_y+=t_v->m_y;
}
void c_b2Vec2::mark(){
	Object::mark();
}
c_b2DestructionListener::c_b2DestructionListener(){
}
void c_b2DestructionListener::p_SayGoodbyeJoint(c_b2Joint* t_joint){
}
void c_b2DestructionListener::p_SayGoodbyeFixture(c_b2Fixture* t_fixture){
}
void c_b2DestructionListener::mark(){
	Object::mark();
}
c_b2DebugDraw::c_b2DebugDraw(){
	m_m_drawFlags=0;
	m_m_drawScale=FLOAT(1.0);
	m_m_fillAlpha=FLOAT(1.0);
	m_m_lineThickness=FLOAT(1.0);
	m_m_alpha=FLOAT(1.0);
	m_m_xformScale=FLOAT(1.0);
}
c_b2DebugDraw* c_b2DebugDraw::m_new(){
	m_m_drawFlags=0;
	return this;
}
void c_b2DebugDraw::p_SetDrawScale(Float t_drawScale){
	m_m_drawScale=t_drawScale;
}
void c_b2DebugDraw::p_SetFillAlpha(Float t_alpha){
	m_m_fillAlpha=t_alpha;
}
void c_b2DebugDraw::p_SetLineThickness(Float t_lineThickness){
	m_m_lineThickness=t_lineThickness;
}
int c_b2DebugDraw::m_e_shapeBit;
int c_b2DebugDraw::m_e_jointBit;
void c_b2DebugDraw::p_SetFlags(int t_flags){
	m_m_drawFlags=t_flags;
}
void c_b2DebugDraw::p_Clear(){
	bb_graphics_Cls(FLOAT(0.0),FLOAT(0.0),FLOAT(0.0));
}
int c_b2DebugDraw::p_GetFlags(){
	return m_m_drawFlags;
}
void c_b2DebugDraw::p_SetAlpha(Float t_alpha){
	m_m_alpha=t_alpha;
}
void c_b2DebugDraw::p_DrawCircle(c_b2Vec2* t_center,Float t_radius,c_b2Color* t_color){
	p_SetAlpha(m_m_alpha);
	bb_graphics_SetColor(Float(t_color->m__r),Float(t_color->m__g),Float(t_color->m__b));
	bb_graphics_DrawCircle(t_center->m_x*m_m_drawScale,t_center->m_y*m_m_drawScale,t_radius*m_m_drawScale);
}
void c_b2DebugDraw::p_DrawSolidCircle(c_b2Vec2* t_center,Float t_radius,c_b2Vec2* t_axis,c_b2Color* t_color){
	p_SetAlpha(m_m_alpha);
	bb_graphics_SetColor(Float(t_color->m__r),Float(t_color->m__g),Float(t_color->m__b));
	p_DrawCircle(t_center,t_radius,t_color);
	bb_graphics_SetColor(FLOAT(64.0),FLOAT(64.0),FLOAT(64.0));
	bb_graphics_DrawLine(t_center->m_x*m_m_drawScale,t_center->m_y*m_m_drawScale,(t_center->m_x+t_axis->m_x*t_radius)*m_m_drawScale,(t_center->m_y+t_axis->m_y*t_radius)*m_m_drawScale);
	bb_graphics_SetColor(FLOAT(255.0),FLOAT(255.0),FLOAT(255.0));
}
void c_b2DebugDraw::p_DrawPolygon(Array<c_b2Vec2* > t_vertices,int t_vertexCount,c_b2Color* t_color){
	p_SetAlpha(m_m_alpha);
	bb_graphics_SetColor(Float(t_color->m__r),Float(t_color->m__g),Float(t_color->m__b));
	int t_i=0;
	for(t_i=0;t_i<t_vertexCount-1;t_i=t_i+1){
		bb_graphics_DrawLine(t_vertices[t_i]->m_x*m_m_drawScale,t_vertices[t_i]->m_y*m_m_drawScale,t_vertices[t_i+1]->m_x*m_m_drawScale,t_vertices[t_i+1]->m_y*m_m_drawScale);
	}
	bb_graphics_DrawLine(t_vertices[t_i]->m_x*m_m_drawScale,t_vertices[t_i]->m_y*m_m_drawScale,t_vertices[0]->m_x*m_m_drawScale,t_vertices[0]->m_y*m_m_drawScale);
}
void c_b2DebugDraw::p_DrawSolidPolygon(Array<c_b2Vec2* > t_vertices,int t_vertexCount,c_b2Color* t_color){
	p_DrawPolygon(t_vertices,t_vertexCount,t_color);
}
void c_b2DebugDraw::p_DrawSegment(c_b2Vec2* t_p1,c_b2Vec2* t_p2,c_b2Color* t_color){
	p_SetAlpha(m_m_alpha);
	bb_graphics_SetColor(Float(t_color->m__r),Float(t_color->m__g),Float(t_color->m__b));
	bb_graphics_DrawLine(t_p1->m_x*m_m_drawScale,t_p1->m_y*m_m_drawScale,t_p2->m_x*m_m_drawScale,t_p2->m_y*m_m_drawScale);
}
int c_b2DebugDraw::m_e_controllerBit;
int c_b2DebugDraw::m_e_pairBit;
int c_b2DebugDraw::m_e_aabbBit;
int c_b2DebugDraw::m_e_centerOfMassBit;
void c_b2DebugDraw::p_DrawTransform(c_b2Transform* t_xf){
	p_SetAlpha(m_m_alpha);
	bb_graphics_SetColor(FLOAT(255.0),FLOAT(0.0),FLOAT(0.0));
	bb_graphics_DrawLine(t_xf->m_position->m_x*m_m_drawScale,t_xf->m_position->m_y*m_m_drawScale,(t_xf->m_position->m_x+m_m_xformScale*t_xf->m_R->m_col1->m_x)*m_m_drawScale,(t_xf->m_position->m_y+m_m_xformScale*t_xf->m_R->m_col1->m_y)*m_m_drawScale);
	bb_graphics_DrawLine(t_xf->m_position->m_x*m_m_drawScale,t_xf->m_position->m_y*m_m_drawScale,(t_xf->m_position->m_x+m_m_xformScale*t_xf->m_R->m_col2->m_x)*m_m_drawScale,(t_xf->m_position->m_y+m_m_xformScale*t_xf->m_R->m_col2->m_y)*m_m_drawScale);
}
void c_b2DebugDraw::mark(){
	Object::mark();
}
c_b2Body::c_b2Body(){
	m_m_flags=0;
	m_m_world=0;
	m_m_xf=(new c_b2Transform)->m_new(0,0);
	m_m_sweep=(new c_b2Sweep)->m_new();
	m_m_jointList=0;
	m_m_controllerList=0;
	m_m_contactList=0;
	m_m_controllerCount=0;
	m_m_prev=0;
	m_m_next=0;
	m_m_linearVelocity=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_m_angularVelocity=FLOAT(.0);
	m_m_linearDamping=FLOAT(.0);
	m_m_angularDamping=FLOAT(.0);
	m_m_force=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_m_torque=FLOAT(.0);
	m_m_sleepTime=FLOAT(.0);
	m_m_type=0;
	m_m_mass=FLOAT(.0);
	m_m_invMass=FLOAT(.0);
	m_m_I=FLOAT(.0);
	m_m_invI=FLOAT(.0);
	m_m_inertiaScale=FLOAT(.0);
	m_m_userData=0;
	m_m_fixtureList=0;
	m_m_fixtureCount=0;
	m_m_islandIndex=0;
}
c_b2Body* c_b2Body::m_new(c_b2BodyDef* t_bd,c_b2World* t_world){
	m_m_flags=0;
	if(t_bd->m_bullet){
		m_m_flags|=8;
	}
	if(t_bd->m_fixedRotation){
		m_m_flags|=16;
	}
	if(t_bd->m_allowSleep){
		m_m_flags|=4;
	}
	if(t_bd->m_awake){
		m_m_flags|=2;
	}
	if(t_bd->m_active){
		m_m_flags|=32;
	}
	gc_assign(m_m_world,t_world);
	m_m_xf->m_position->p_SetV(t_bd->m_position);
	m_m_xf->m_R->p_Set6(t_bd->m_angle);
	m_m_sweep->m_localCenter->p_SetZero();
	m_m_sweep->m_t0=FLOAT(1.0);
	m_m_sweep->m_a0=t_bd->m_angle;
	m_m_sweep->m_a=t_bd->m_angle;
	c_b2Mat22* t_tMat=m_m_xf->m_R;
	c_b2Vec2* t_tVec=m_m_sweep->m_localCenter;
	m_m_sweep->m_c->m_x=t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y;
	m_m_sweep->m_c->m_y=t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y;
	m_m_sweep->m_c->m_x+=m_m_xf->m_position->m_x;
	m_m_sweep->m_c->m_y+=m_m_xf->m_position->m_y;
	m_m_sweep->m_c0->p_SetV(m_m_sweep->m_c);
	m_m_jointList=0;
	m_m_controllerList=0;
	m_m_contactList=0;
	m_m_controllerCount=0;
	m_m_prev=0;
	m_m_next=0;
	m_m_linearVelocity->p_SetV(t_bd->m_linearVelocity);
	m_m_angularVelocity=t_bd->m_angularVelocity;
	m_m_linearDamping=t_bd->m_linearDamping;
	m_m_angularDamping=t_bd->m_angularDamping;
	m_m_force->p_Set2(FLOAT(0.0),FLOAT(0.0));
	m_m_torque=FLOAT(0.0);
	m_m_sleepTime=FLOAT(0.0);
	m_m_type=t_bd->m_type;
	if(m_m_type==2){
		m_m_mass=FLOAT(1.0);
		m_m_invMass=FLOAT(1.0);
	}else{
		m_m_mass=FLOAT(0.0);
		m_m_invMass=FLOAT(0.0);
	}
	m_m_I=FLOAT(0.0);
	m_m_invI=FLOAT(0.0);
	m_m_inertiaScale=t_bd->m_inertiaScale;
	gc_assign(m_m_userData,t_bd->m_userData);
	m_m_fixtureList=0;
	m_m_fixtureCount=0;
	return this;
}
c_b2Body* c_b2Body::m_new2(){
	return this;
}
void c_b2Body::p_SetAwake(bool t_flag){
	if(t_flag){
		m_m_flags|=2;
		m_m_sleepTime=FLOAT(0.0);
	}else{
		m_m_flags&=-3;
		m_m_sleepTime=FLOAT(0.0);
		m_m_linearVelocity->p_SetZero();
		m_m_angularVelocity=FLOAT(0.0);
		m_m_force->p_SetZero();
		m_m_torque=FLOAT(0.0);
	}
}
c_b2ContactEdge* c_b2Body::p_GetContactList(){
	return m_m_contactList;
}
void c_b2Body::p_ResetMassData(){
	m_m_mass=FLOAT(0.0);
	m_m_invMass=FLOAT(0.0);
	m_m_I=FLOAT(0.0);
	m_m_invI=FLOAT(0.0);
	m_m_sweep->m_localCenter->p_SetZero();
	if(m_m_type==0 || m_m_type==1){
		return;
	}
	c_b2Vec2* t_center=c_b2Vec2::m_Make(FLOAT(0.0),FLOAT(0.0));
	c_b2Fixture* t_f=m_m_fixtureList;
	while(t_f!=0){
		if(t_f->m_m_density==FLOAT(0.0)){
			t_f=t_f->m_m_next;
			continue;
		}
		c_b2MassData* t_massData=t_f->p_GetMassData(0);
		m_m_mass+=t_massData->m_mass;
		t_center->m_x+=t_massData->m_center->m_x*t_massData->m_mass;
		t_center->m_y+=t_massData->m_center->m_y*t_massData->m_mass;
		m_m_I+=t_massData->m_I;
		t_f=t_f->m_m_next;
	}
	if(m_m_mass>FLOAT(0.0)){
		m_m_invMass=FLOAT(1.0)/m_m_mass;
		t_center->m_x*=m_m_invMass;
		t_center->m_y*=m_m_invMass;
	}else{
		m_m_mass=FLOAT(1.0);
		m_m_invMass=FLOAT(1.0);
	}
	if(m_m_I>FLOAT(0.0) && (m_m_flags&16)==0){
		m_m_I-=m_m_mass*(t_center->m_x*t_center->m_x+t_center->m_y*t_center->m_y);
		m_m_I*=m_m_inertiaScale;
		m_m_invI=FLOAT(1.0)/m_m_I;
	}else{
		m_m_I=FLOAT(0.0);
		m_m_invI=FLOAT(0.0);
	}
	c_b2Vec2* t_oldCenter=m_m_sweep->m_c->p_Copy();
	m_m_sweep->m_localCenter->p_SetV(t_center);
	c_b2Math::m_MulX(m_m_xf,m_m_sweep->m_localCenter,m_m_sweep->m_c0);
	m_m_sweep->m_c->p_SetV(m_m_sweep->m_c0);
	m_m_linearVelocity->m_x+=m_m_angularVelocity*-(m_m_sweep->m_c->m_y-t_oldCenter->m_y);
	m_m_linearVelocity->m_y+=m_m_angularVelocity*+(m_m_sweep->m_c->m_x-t_oldCenter->m_x);
}
c_b2Fixture* c_b2Body::p_CreateFixture(c_b2FixtureDef* t_def){
	if(m_m_world->p_IsLocked()==true){
		return 0;
	}
	c_b2Fixture* t_fixture=(new c_b2Fixture)->m_new();
	t_fixture->p_Create3(this,m_m_xf,t_def);
	if((m_m_flags&32)!=0){
		c_IBroadPhase* t_broadPhase=m_m_world->m_m_contactManager->m_m_broadPhase;
		t_fixture->p_CreateProxy2(t_broadPhase,m_m_xf);
	}
	gc_assign(t_fixture->m_m_next,m_m_fixtureList);
	gc_assign(m_m_fixtureList,t_fixture);
	m_m_fixtureCount+=1;
	gc_assign(t_fixture->m_m_body,this);
	if(t_fixture->m_m_density>FLOAT(0.0)){
		p_ResetMassData();
	}
	m_m_world->m_m_flags|=1;
	return t_fixture;
}
Float c_b2Body::p_GetAngularVelocity(){
	return m_m_angularVelocity;
}
void c_b2Body::p_SetAngularVelocity(Float t_omega){
	if(m_m_type==0){
		return;
	}
	m_m_angularVelocity=t_omega;
}
void c_b2Body::p_SetLinearVelocity(c_b2Vec2* t_v){
	if(m_m_type==0){
		return;
	}
	m_m_linearVelocity->p_SetV(t_v);
}
c_b2Vec2* c_b2Body::p_GetPosition(){
	return m_m_xf->m_position;
}
void c_b2Body::p_SetPositionAndAngle(c_b2Vec2* t_position,Float t_angle){
	c_b2Fixture* t_f=0;
	if(m_m_world->p_IsLocked()==true){
		return;
	}
	m_m_xf->m_R->p_Set6(t_angle);
	m_m_xf->m_position->p_SetV(t_position);
	c_b2Mat22* t_tMat=m_m_xf->m_R;
	c_b2Vec2* t_tVec=m_m_sweep->m_localCenter;
	m_m_sweep->m_c->m_x=t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y;
	m_m_sweep->m_c->m_y=t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y;
	m_m_sweep->m_c->m_x+=m_m_xf->m_position->m_x;
	m_m_sweep->m_c->m_y+=m_m_xf->m_position->m_y;
	m_m_sweep->m_c0->p_SetV(m_m_sweep->m_c);
	m_m_sweep->m_a0=t_angle;
	m_m_sweep->m_a=t_angle;
	c_IBroadPhase* t_broadPhase=m_m_world->m_m_contactManager->m_m_broadPhase;
	t_f=m_m_fixtureList;
	while(t_f!=0){
		t_f->p_Synchronize(t_broadPhase,m_m_xf,m_m_xf);
		t_f=t_f->m_m_next;
	}
	m_m_world->m_m_contactManager->p_FindNewContacts();
}
void c_b2Body::p_SetAngle(Float t_angle){
	p_SetPositionAndAngle(p_GetPosition(),t_angle);
}
Float c_b2Body::p_GetAngle(){
	return m_m_sweep->m_a;
}
void c_b2Body::p_SetPosition(c_b2Vec2* t_position){
	p_SetPositionAndAngle(t_position,p_GetAngle());
}
bool c_b2Body::p_IsAwake(){
	return (m_m_flags&2)==2;
}
bool c_b2Body::p_ShouldCollide(c_b2Body* t_other){
	if(m_m_type!=2 && t_other->m_m_type!=2){
		return false;
	}
	c_b2JointEdge* t_jn=m_m_jointList;
	while(t_jn!=0){
		if(t_jn->m_other==t_other && t_jn->m_joint->m_m_collideConnected==false){
			return false;
		}
		t_jn=t_jn->m_nextItem;
	}
	return true;
}
c_b2Transform* c_b2Body::p_GetTransform(){
	return m_m_xf;
}
int c_b2Body::p_GetType(){
	return m_m_type;
}
bool c_b2Body::p_IsBullet(){
	return (m_m_flags&8)==8;
}
void c_b2Body::p_SynchronizeTransform(){
	c_b2Mat22* t_tMat=m_m_xf->m_R;
	t_tMat->p_Set6(m_m_sweep->m_a);
	c_b2Vec2* t_tVec=m_m_sweep->m_localCenter;
	m_m_xf->m_position->m_x=m_m_sweep->m_c->m_x-(t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y);
	m_m_xf->m_position->m_y=m_m_sweep->m_c->m_y-(t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y);
}
c_b2Transform* c_b2Body::m_s_xf1;
void c_b2Body::p_SynchronizeFixtures(){
	c_b2Transform* t_xf1=m_s_xf1;
	t_xf1->m_R->p_Set6(m_m_sweep->m_a0);
	c_b2Mat22* t_tMat=t_xf1->m_R;
	c_b2Vec2* t_tVec=m_m_sweep->m_localCenter;
	t_xf1->m_position->m_x=m_m_sweep->m_c0->m_x-(t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y);
	t_xf1->m_position->m_y=m_m_sweep->m_c0->m_y-(t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y);
	c_b2Fixture* t_f=0;
	c_IBroadPhase* t_broadPhase=m_m_world->m_m_contactManager->m_m_broadPhase;
	t_f=m_m_fixtureList;
	while(t_f!=0){
		t_f->p_Synchronize(t_broadPhase,t_xf1,m_m_xf);
		t_f=t_f->m_m_next;
	}
}
void c_b2Body::p_Advance(Float t_t){
	m_m_sweep->p_Advance(t_t);
	m_m_sweep->m_c->p_SetV(m_m_sweep->m_c0);
	m_m_sweep->m_a=m_m_sweep->m_a0;
	p_SynchronizeTransform();
}
bool c_b2Body::p_IsActive(){
	return (m_m_flags&32)==32;
}
c_b2Fixture* c_b2Body::p_GetFixtureList(){
	return m_m_fixtureList;
}
c_b2Body* c_b2Body::p_GetNext(){
	return m_m_next;
}
c_b2Vec2* c_b2Body::p_GetWorldCenter(){
	return m_m_sweep->m_c;
}
void c_b2Body::p_GetWorldPoint(c_b2Vec2* t_localPoint,c_b2Vec2* t_out){
	c_b2Mat22* t_A=m_m_xf->m_R;
	Float t_tmp=t_localPoint->m_x;
	t_out->p_Set2(t_A->m_col1->m_x*t_tmp+t_A->m_col2->m_x*t_localPoint->m_y,t_A->m_col1->m_y*t_tmp+t_A->m_col2->m_y*t_localPoint->m_y);
	t_out->m_x+=m_m_xf->m_position->m_x;
	t_out->m_y+=m_m_xf->m_position->m_y;
}
void c_b2Body::mark(){
	Object::mark();
	gc_mark_q(m_m_world);
	gc_mark_q(m_m_xf);
	gc_mark_q(m_m_sweep);
	gc_mark_q(m_m_jointList);
	gc_mark_q(m_m_controllerList);
	gc_mark_q(m_m_contactList);
	gc_mark_q(m_m_prev);
	gc_mark_q(m_m_next);
	gc_mark_q(m_m_linearVelocity);
	gc_mark_q(m_m_force);
	gc_mark_q(m_m_userData);
	gc_mark_q(m_m_fixtureList);
}
c_b2Contact::c_b2Contact(){
	m_m_flags=0;
	m_m_fixtureA=0;
	m_m_fixtureB=0;
	m_m_prev=0;
	m_m_next=0;
	m_m_nodeA=(new c_b2ContactEdge)->m_new();
	m_m_nodeB=(new c_b2ContactEdge)->m_new();
	m_m_manifold=(new c_b2Manifold)->m_new();
	m_m_swapped=false;
	m_m_oldManifold=(new c_b2Manifold)->m_new();
	m_m_toi=FLOAT(.0);
}
void c_b2Contact::p_FlagForFiltering(){
	m_m_flags|=64;
}
c_b2Fixture* c_b2Contact::p_GetFixtureA(){
	return m_m_fixtureA;
}
c_b2Fixture* c_b2Contact::p_GetFixtureB(){
	return m_m_fixtureB;
}
bool c_b2Contact::p_IsTouching(){
	return (m_m_flags&16)==16;
}
c_b2Contact* c_b2Contact::p_GetNext(){
	return m_m_next;
}
void c_b2Contact::p_Evaluate(){
}
void c_b2Contact::p_Update2(c_b2ContactListenerInterface* t_listener){
	c_b2Manifold* t_tManifold=m_m_oldManifold;
	gc_assign(m_m_oldManifold,m_m_manifold);
	gc_assign(m_m_manifold,t_tManifold);
	m_m_flags|=32;
	bool t_touching=false;
	bool t_wasTouching=(m_m_flags&16)==16;
	c_b2Body* t_bodyA=m_m_fixtureA->m_m_body;
	c_b2Body* t_bodyB=m_m_fixtureB->m_m_body;
	bool t_aabbOverlap=m_m_fixtureA->m_m_aabb->p_TestOverlap2(m_m_fixtureB->m_m_aabb);
	if((m_m_flags&1)!=0){
		if(t_aabbOverlap){
			c_b2Shape* t_shapeA=m_m_fixtureA->p_GetShape();
			c_b2Shape* t_shapeB=m_m_fixtureB->p_GetShape();
			c_b2Transform* t_xfA=t_bodyA->p_GetTransform();
			c_b2Transform* t_xfB=t_bodyB->p_GetTransform();
			t_touching=c_b2Shape::m_TestOverlap(t_shapeA,t_xfA,t_shapeB,t_xfB);
		}
		m_m_manifold->m_m_pointCount=0;
	}else{
		if(t_bodyA->p_GetType()!=2 || t_bodyA->p_IsBullet() || t_bodyB->p_GetType()!=2 || t_bodyB->p_IsBullet()){
			m_m_flags|=2;
		}else{
			m_m_flags&=-3;
		}
		if(t_aabbOverlap){
			p_Evaluate();
			t_touching=m_m_manifold->m_m_pointCount>0;
			for(int t_i=0;t_i<m_m_manifold->m_m_pointCount;t_i=t_i+1){
				c_b2ManifoldPoint* t_mp2=m_m_manifold->m_m_points[t_i];
				t_mp2->m_m_normalImpulse=FLOAT(0.0);
				t_mp2->m_m_tangentImpulse=FLOAT(0.0);
				c_b2ContactID* t_id2=t_mp2->m_m_id;
				for(int t_j=0;t_j<m_m_oldManifold->m_m_pointCount;t_j=t_j+1){
					c_b2ManifoldPoint* t_mp1=m_m_oldManifold->m_m_points[t_j];
					if(t_mp1->m_m_id->p_Key()==t_id2->p_Key()){
						t_mp2->m_m_normalImpulse=t_mp1->m_m_normalImpulse;
						t_mp2->m_m_tangentImpulse=t_mp1->m_m_tangentImpulse;
						break;
					}
				}
			}
		}else{
			m_m_manifold->m_m_pointCount=0;
		}
		if(t_touching!=t_wasTouching){
			t_bodyA->p_SetAwake(true);
			t_bodyB->p_SetAwake(true);
		}
	}
	if(t_touching){
		m_m_flags|=16;
	}else{
		m_m_flags&=-17;
	}
	if(t_wasTouching==false && t_touching==true){
		t_listener->p_BeginContact(this);
	}
	if(t_wasTouching==true && t_touching==false){
		t_listener->p_EndContact(this);
	}
	if((m_m_flags&1)==0){
		t_listener->p_PreSolve(this,m_m_oldManifold);
	}
}
c_b2Manifold* c_b2Contact::p_GetManifold(){
	return m_m_manifold;
}
c_b2TOIInput* c_b2Contact::m_s_input;
Float c_b2Contact::p_ComputeTOI(c_b2Sweep* t_sweepA,c_b2Sweep* t_sweepB){
	m_s_input->m_proxyA->p_Set5(m_m_fixtureA->p_GetShape());
	m_s_input->m_proxyB->p_Set5(m_m_fixtureB->p_GetShape());
	gc_assign(m_s_input->m_sweepA,t_sweepA);
	gc_assign(m_s_input->m_sweepB,t_sweepB);
	m_s_input->m_tolerance=FLOAT(0.005);
	return c_b2TimeOfImpact::m_TimeOfImpact(m_s_input);
}
void c_b2Contact::p_Reset(c_b2Fixture* t_fixtureA,c_b2Fixture* t_fixtureB){
	m_m_flags=32;
	if(!((t_fixtureA)!=0) || !((t_fixtureB)!=0)){
		m_m_fixtureA=0;
		m_m_fixtureB=0;
		return;
	}
	if(t_fixtureA->p_IsSensor() || t_fixtureB->p_IsSensor()){
		m_m_flags|=1;
	}
	c_b2Body* t_bodyA=t_fixtureA->p_GetBody();
	c_b2Body* t_bodyB=t_fixtureB->p_GetBody();
	if(t_bodyA->p_GetType()!=2 || t_bodyA->p_IsBullet() || t_bodyB->p_GetType()!=2 || t_bodyB->p_IsBullet()){
		m_m_flags|=2;
	}
	gc_assign(m_m_fixtureA,t_fixtureA);
	gc_assign(m_m_fixtureB,t_fixtureB);
	m_m_manifold->m_m_pointCount=0;
	m_m_prev=0;
	m_m_next=0;
	m_m_nodeA->m_contact=0;
	m_m_nodeA->m_prevItem=0;
	m_m_nodeA->m_nextItem=0;
	m_m_nodeA->m_other=0;
	m_m_nodeB->m_contact=0;
	m_m_nodeB->m_prevItem=0;
	m_m_nodeB->m_nextItem=0;
	m_m_nodeB->m_other=0;
}
c_b2Contact* c_b2Contact::m_new(){
	return this;
}
void c_b2Contact::mark(){
	Object::mark();
	gc_mark_q(m_m_fixtureA);
	gc_mark_q(m_m_fixtureB);
	gc_mark_q(m_m_prev);
	gc_mark_q(m_m_next);
	gc_mark_q(m_m_nodeA);
	gc_mark_q(m_m_nodeB);
	gc_mark_q(m_m_manifold);
	gc_mark_q(m_m_oldManifold);
}
c_b2Joint::c_b2Joint(){
	m_m_collideConnected=false;
	m_m_prev=0;
	m_m_next=0;
	m_m_bodyA=0;
	m_m_bodyB=0;
	m_m_edgeA=(new c_b2JointEdge)->m_new();
	m_m_edgeB=(new c_b2JointEdge)->m_new();
	m_m_islandFlag=false;
	m_m_type=0;
}
void c_b2Joint::m_Destroy(c_b2Joint* t_joint,Object* t_allocator){
}
void c_b2Joint::p_InitVelocityConstraints(c_b2TimeStep* t_timeStep){
}
void c_b2Joint::p_SolveVelocityConstraints(c_b2TimeStep* t_timeStep){
}
void c_b2Joint::p_FinalizeVelocityConstraints(){
}
bool c_b2Joint::p_SolvePositionConstraints(Float t_baumgarte){
	return false;
}
c_b2Body* c_b2Joint::p_GetBodyA(){
	return m_m_bodyA;
}
c_b2Body* c_b2Joint::p_GetBodyB(){
	return m_m_bodyB;
}
void c_b2Joint::p_GetAnchorA(c_b2Vec2* t_out){
}
void c_b2Joint::p_GetAnchorB(c_b2Vec2* t_out){
}
void c_b2Joint::mark(){
	Object::mark();
	gc_mark_q(m_m_prev);
	gc_mark_q(m_m_next);
	gc_mark_q(m_m_bodyA);
	gc_mark_q(m_m_bodyB);
	gc_mark_q(m_m_edgeA);
	gc_mark_q(m_m_edgeB);
}
c_b2Controller::c_b2Controller(){
	m_m_bodyList=0;
	m_m_bodyCount=0;
	m_m_next=0;
}
void c_b2Controller::p_RemoveBody(c_b2Body* t_body){
	c_b2ControllerEdge* t_edge=t_body->m_m_controllerList;
	while(((t_edge)!=0) && t_edge->m_controller!=this){
		t_edge=t_edge->m_nextController;
	}
	if((t_edge->m_prevBody)!=0){
		gc_assign(t_edge->m_prevBody->m_nextBody,t_edge->m_nextBody);
	}
	if((t_edge->m_nextBody)!=0){
		gc_assign(t_edge->m_nextBody->m_prevBody,t_edge->m_prevBody);
	}
	if((t_edge->m_nextController)!=0){
		gc_assign(t_edge->m_nextController->m_prevController,t_edge->m_prevController);
	}
	if((t_edge->m_prevController)!=0){
		gc_assign(t_edge->m_prevController->m_nextController,t_edge->m_nextController);
	}
	if(m_m_bodyList==t_edge){
		gc_assign(m_m_bodyList,t_edge->m_nextBody);
	}
	if(t_body->m_m_controllerList==t_edge){
		gc_assign(t_body->m_m_controllerList,t_edge->m_nextController);
	}
	t_body->m_m_controllerCount-=1;
	m_m_bodyCount-=1;
}
void c_b2Controller::p_TimeStep2(c_b2TimeStep* t_timeStep){
}
void c_b2Controller::p_Draw(c_b2DebugDraw* t_debugDraw){
}
void c_b2Controller::mark(){
	Object::mark();
	gc_mark_q(m_m_bodyList);
	gc_mark_q(m_m_next);
}
c_b2ContactManager::c_b2ContactManager(){
	m_m_world=0;
	m_m_contactCount=0;
	m_m_contactFilter=0;
	m_m_contactListener=0;
	m_m_allocator=0;
	m_m_contactFactory=0;
	m_m_broadPhase=0;
	m_pairsCallback=0;
}
c_b2ContactManager* c_b2ContactManager::m_new(){
	m_m_world=0;
	m_m_contactCount=0;
	gc_assign(m_m_contactFilter,c_b2ContactFilter::m_b2_defaultFilter);
	gc_assign(m_m_contactListener,c_b2ContactListener::m_b2_defaultListener);
	gc_assign(m_m_contactFactory,(new c_b2ContactFactory)->m_new(m_m_allocator));
	gc_assign(m_m_broadPhase,((new c_b2DynamicTreeBroadPhase)->m_new()));
	gc_assign(m_pairsCallback,((new c_CMUpdatePairsCallback)->m_new(this)));
	return this;
}
void c_b2ContactManager::p_Destroy(c_b2Contact* t_c){
	c_b2Fixture* t_fixtureA=t_c->p_GetFixtureA();
	c_b2Fixture* t_fixtureB=t_c->p_GetFixtureB();
	c_b2Body* t_bodyA=t_fixtureA->p_GetBody();
	c_b2Body* t_bodyB=t_fixtureB->p_GetBody();
	if(t_c->p_IsTouching()){
		m_m_contactListener->p_EndContact(t_c);
	}
	if((t_c->m_m_prev)!=0){
		gc_assign(t_c->m_m_prev->m_m_next,t_c->m_m_next);
	}
	if((t_c->m_m_next)!=0){
		gc_assign(t_c->m_m_next->m_m_prev,t_c->m_m_prev);
	}
	if(t_c==m_m_world->m_m_contactList){
		gc_assign(m_m_world->m_m_contactList,t_c->m_m_next);
	}
	if((t_c->m_m_nodeA->m_prevItem)!=0){
		gc_assign(t_c->m_m_nodeA->m_prevItem->m_nextItem,t_c->m_m_nodeA->m_nextItem);
	}
	if((t_c->m_m_nodeA->m_nextItem)!=0){
		gc_assign(t_c->m_m_nodeA->m_nextItem->m_prevItem,t_c->m_m_nodeA->m_prevItem);
	}
	if(t_c->m_m_nodeA==t_bodyA->m_m_contactList){
		gc_assign(t_bodyA->m_m_contactList,t_c->m_m_nodeA->m_nextItem);
	}
	if((t_c->m_m_nodeB->m_prevItem)!=0){
		gc_assign(t_c->m_m_nodeB->m_prevItem->m_nextItem,t_c->m_m_nodeB->m_nextItem);
	}
	if((t_c->m_m_nodeB->m_nextItem)!=0){
		gc_assign(t_c->m_m_nodeB->m_nextItem->m_prevItem,t_c->m_m_nodeB->m_prevItem);
	}
	if(t_c->m_m_nodeB==t_bodyB->m_m_contactList){
		gc_assign(t_bodyB->m_m_contactList,t_c->m_m_nodeB->m_nextItem);
	}
	m_m_contactFactory->p_Destroy(t_c);
	m_m_world->m_m_contactCount-=1;
}
void c_b2ContactManager::p_FindNewContacts(){
	m_m_broadPhase->p_UpdatePairs(m_pairsCallback);
}
void c_b2ContactManager::p_Collide(){
	c_b2Contact* t_c=m_m_world->m_m_contactList;
	while(t_c!=0){
		c_b2Fixture* t_fixtureA=t_c->p_GetFixtureA();
		c_b2Fixture* t_fixtureB=t_c->p_GetFixtureB();
		c_b2Body* t_bodyA=t_fixtureA->p_GetBody();
		c_b2Body* t_bodyB=t_fixtureB->p_GetBody();
		c_b2Contact* t_cNuke=0;
		if(t_bodyA->p_IsAwake()==false && t_bodyB->p_IsAwake()==false){
			t_c=t_c->p_GetNext();
			continue;
		}
		if((t_c->m_m_flags&64)!=0){
			if(t_bodyB->p_ShouldCollide(t_bodyA)==false){
				t_cNuke=t_c;
				t_c=t_cNuke->p_GetNext();
				p_Destroy(t_cNuke);
				continue;
			}
			if(m_m_contactFilter->p_ShouldCollide2(t_fixtureA,t_fixtureB)==false){
				t_cNuke=t_c;
				t_c=t_cNuke->p_GetNext();
				p_Destroy(t_cNuke);
				continue;
			}
			t_c->m_m_flags&=-65;
		}
		Object* t_proxyA=t_fixtureA->m_m_proxy;
		Object* t_proxyB=t_fixtureB->m_m_proxy;
		bool t_overlap=m_m_broadPhase->p_TestOverlap(t_proxyA,t_proxyB);
		if(t_overlap==false){
			t_cNuke=t_c;
			t_c=t_cNuke->p_GetNext();
			p_Destroy(t_cNuke);
			continue;
		}
		t_c->p_Update2(m_m_contactListener);
		t_c=t_c->p_GetNext();
	}
}
void c_b2ContactManager::p_AddPair(Object* t_proxyUserDataA,Object* t_proxyUserDataB){
	c_b2Fixture* t_fixtureA=dynamic_cast<c_b2Fixture*>(t_proxyUserDataA);
	c_b2Fixture* t_fixtureB=dynamic_cast<c_b2Fixture*>(t_proxyUserDataB);
	c_b2Body* t_bodyA=t_fixtureA->p_GetBody();
	c_b2Body* t_bodyB=t_fixtureB->p_GetBody();
	if(t_bodyA==t_bodyB){
		return;
	}
	c_b2ContactEdge* t_edge=t_bodyB->p_GetContactList();
	while((t_edge)!=0){
		if(t_edge->m_other==t_bodyA){
			c_b2Fixture* t_fA=t_edge->m_contact->p_GetFixtureA();
			c_b2Fixture* t_fB=t_edge->m_contact->p_GetFixtureB();
			if(t_fA==t_fixtureA && t_fB==t_fixtureB || t_fA==t_fixtureB && t_fB==t_fixtureA){
				return;
			}
		}
		t_edge=t_edge->m_nextItem;
	}
	if(t_bodyB->p_ShouldCollide(t_bodyA)==false){
		return;
	}
	if(m_m_contactFilter->p_ShouldCollide2(t_fixtureA,t_fixtureB)==false){
		return;
	}
	c_b2Contact* t_c=m_m_contactFactory->p_Create(t_fixtureA,t_fixtureB);
	t_fixtureA=t_c->p_GetFixtureA();
	t_fixtureB=t_c->p_GetFixtureB();
	t_bodyA=t_fixtureA->m_m_body;
	t_bodyB=t_fixtureB->m_m_body;
	t_c->m_m_prev=0;
	gc_assign(t_c->m_m_next,m_m_world->m_m_contactList);
	if(m_m_world->m_m_contactList!=0){
		gc_assign(m_m_world->m_m_contactList->m_m_prev,t_c);
	}
	gc_assign(m_m_world->m_m_contactList,t_c);
	gc_assign(t_c->m_m_nodeA->m_contact,t_c);
	gc_assign(t_c->m_m_nodeA->m_other,t_bodyB);
	t_c->m_m_nodeA->m_prevItem=0;
	gc_assign(t_c->m_m_nodeA->m_nextItem,t_bodyA->m_m_contactList);
	if(t_bodyA->m_m_contactList!=0){
		gc_assign(t_bodyA->m_m_contactList->m_prevItem,t_c->m_m_nodeA);
	}
	gc_assign(t_bodyA->m_m_contactList,t_c->m_m_nodeA);
	gc_assign(t_c->m_m_nodeB->m_contact,t_c);
	gc_assign(t_c->m_m_nodeB->m_other,t_bodyA);
	t_c->m_m_nodeB->m_prevItem=0;
	gc_assign(t_c->m_m_nodeB->m_nextItem,t_bodyB->m_m_contactList);
	if(t_bodyB->m_m_contactList!=0){
		gc_assign(t_bodyB->m_m_contactList->m_prevItem,t_c->m_m_nodeB);
	}
	gc_assign(t_bodyB->m_m_contactList,t_c->m_m_nodeB);
	m_m_world->m_m_contactCount+=1;
	return;
}
void c_b2ContactManager::mark(){
	Object::mark();
	gc_mark_q(m_m_world);
	gc_mark_q(m_m_contactFilter);
	gc_mark_q(m_m_contactListener);
	gc_mark_q(m_m_allocator);
	gc_mark_q(m_m_contactFactory);
	gc_mark_q(m_m_broadPhase);
	gc_mark_q(m_pairsCallback);
}
c_b2ContactFilter::c_b2ContactFilter(){
}
c_b2ContactFilter* c_b2ContactFilter::m_new(){
	return this;
}
c_b2ContactFilter* c_b2ContactFilter::m_b2_defaultFilter;
bool c_b2ContactFilter::p_ShouldCollide2(c_b2Fixture* t_fixtureA,c_b2Fixture* t_fixtureB){
	c_b2FilterData* t_filter1=t_fixtureA->p_GetFilterData();
	c_b2FilterData* t_filter2=t_fixtureB->p_GetFilterData();
	if(t_filter1->m_groupIndex==t_filter2->m_groupIndex && t_filter1->m_groupIndex!=0){
		return t_filter1->m_groupIndex>0;
	}
	bool t_collide=(t_filter1->m_maskBits&t_filter2->m_categoryBits)!=0 && (t_filter1->m_categoryBits&t_filter2->m_maskBits)!=0;
	return t_collide;
}
void c_b2ContactFilter::mark(){
	Object::mark();
}
c_b2ContactListener::c_b2ContactListener(){
}
c_b2ContactListener* c_b2ContactListener::m_new(){
	return this;
}
c_b2ContactListenerInterface* c_b2ContactListener::m_b2_defaultListener;
void c_b2ContactListener::p_BeginContact(c_b2Contact* t_contact){
}
void c_b2ContactListener::p_EndContact(c_b2Contact* t_contact){
}
void c_b2ContactListener::p_PreSolve(c_b2Contact* t_contact,c_b2Manifold* t_oldManifold){
}
void c_b2ContactListener::p_PostSolve(c_b2Contact* t_contact,c_b2ContactImpulse* t_impulse){
}
void c_b2ContactListener::mark(){
	Object::mark();
}
c_b2ContactFactory::c_b2ContactFactory(){
	m_m_allocator=0;
	m_m_registers=0;
}
void c_b2ContactFactory::p_AddType(c_ContactTypeFactory* t_contactTypeFactory,int t_type1,int t_type2){
	gc_assign(m_m_registers->p_Get(t_type1)->p_Get(t_type2)->m_contactTypeFactory,t_contactTypeFactory);
	m_m_registers->p_Get(t_type1)->p_Get(t_type2)->m_primary=true;
	if(t_type1!=t_type2){
		gc_assign(m_m_registers->p_Get(t_type2)->p_Get(t_type1)->m_contactTypeFactory,t_contactTypeFactory);
		m_m_registers->p_Get(t_type2)->p_Get(t_type1)->m_primary=false;
	}
}
void c_b2ContactFactory::p_InitializeRegisters(){
	gc_assign(m_m_registers,(new c_FlashArray2)->m_new(3));
	for(int t_i=0;t_i<3;t_i=t_i+1){
		m_m_registers->p_Set4(t_i,(new c_FlashArray)->m_new(3));
		for(int t_j=0;t_j<3;t_j=t_j+1){
			m_m_registers->p_Get(t_i)->p_Set3(t_j,(new c_b2ContactRegister)->m_new());
		}
	}
	p_AddType(((new c_CircleContactTypeFactory)->m_new()),0,0);
	p_AddType(((new c_PolyAndCircleContactTypeFactory)->m_new()),1,0);
	p_AddType(((new c_PolygonContactTypeFactory)->m_new()),1,1);
	p_AddType(((new c_EdgeAndCircleContactTypeFactory)->m_new()),2,0);
	p_AddType(((new c_PolyAndEdgeContactTypeFactory)->m_new()),1,2);
}
c_b2ContactFactory* c_b2ContactFactory::m_new(Object* t_allocator){
	gc_assign(m_m_allocator,t_allocator);
	p_InitializeRegisters();
	return this;
}
c_b2ContactFactory* c_b2ContactFactory::m_new2(){
	return this;
}
void c_b2ContactFactory::p_Destroy(c_b2Contact* t_contact){
	if(t_contact->m_m_manifold->m_m_pointCount>0){
		t_contact->m_m_fixtureA->m_m_body->p_SetAwake(true);
		t_contact->m_m_fixtureB->m_m_body->p_SetAwake(true);
	}
	int t_type1=t_contact->m_m_fixtureA->p_GetType();
	int t_type2=t_contact->m_m_fixtureB->p_GetType();
	c_b2ContactRegister* t_reg=0;
	if(t_contact->m_m_swapped){
		t_reg=m_m_registers->p_Get(t_type2)->p_Get(t_type1);
	}else{
		t_reg=m_m_registers->p_Get(t_type1)->p_Get(t_type2);
	}
	if(true){
		t_reg->m_poolCount+=1;
		gc_assign(t_contact->m_m_next,t_reg->m_pool);
		gc_assign(t_reg->m_pool,t_contact);
	}
	c_ContactTypeFactory* t_contactTypeFactory=t_reg->m_contactTypeFactory;
	t_contactTypeFactory->p_Destroy2(t_contact,m_m_allocator);
}
c_b2Contact* c_b2ContactFactory::p_Create(c_b2Fixture* t_fixtureA,c_b2Fixture* t_fixtureB){
	int t_type1=t_fixtureA->p_GetType();
	int t_type2=t_fixtureB->p_GetType();
	c_b2ContactRegister* t_reg=m_m_registers->p_Get(t_type1)->p_Get(t_type2);
	c_b2Contact* t_c=0;
	if((t_reg->m_pool)!=0){
		t_c=t_reg->m_pool;
		gc_assign(t_reg->m_pool,t_c->m_m_next);
		t_reg->m_poolCount-=1;
		if(t_c->m_m_swapped){
			t_c->p_Reset(t_fixtureB,t_fixtureA);
		}else{
			t_c->p_Reset(t_fixtureA,t_fixtureB);
		}
		return t_c;
	}
	c_ContactTypeFactory* t_contactTypeFactory=t_reg->m_contactTypeFactory;
	if(t_contactTypeFactory!=0){
		if(t_reg->m_primary){
			t_c=t_contactTypeFactory->p_Create2(m_m_allocator);
			t_c->p_Reset(t_fixtureA,t_fixtureB);
			t_c->m_m_swapped=false;
			return t_c;
		}else{
			t_c=t_contactTypeFactory->p_Create2(m_m_allocator);
			t_c->p_Reset(t_fixtureB,t_fixtureA);
			t_c->m_m_swapped=true;
			return t_c;
		}
	}else{
		return 0;
	}
}
void c_b2ContactFactory::mark(){
	Object::mark();
	gc_mark_q(m_m_allocator);
	gc_mark_q(m_m_registers);
}
c_b2ContactRegister::c_b2ContactRegister(){
	m_contactTypeFactory=0;
	m_primary=false;
	m_poolCount=0;
	m_pool=0;
}
c_b2ContactRegister* c_b2ContactRegister::m_new(){
	return this;
}
void c_b2ContactRegister::mark(){
	Object::mark();
	gc_mark_q(m_contactTypeFactory);
	gc_mark_q(m_pool);
}
c_FlashArray::c_FlashArray(){
	m_length=0;
	m_arrLength=100;
	m_arr=Array<c_b2ContactRegister* >(100);
}
int c_FlashArray::p_Length(){
	return m_length;
}
void c_FlashArray::p_Length2(int t_value){
	m_length=t_value;
	if(m_length>m_arrLength){
		m_arrLength=m_length;
		gc_assign(m_arr,m_arr.Resize(m_length));
	}
}
c_FlashArray* c_FlashArray::m_new(int t_length){
	p_Length2(t_length);
	return this;
}
c_FlashArray* c_FlashArray::m_new2(Array<c_b2ContactRegister* > t_vals){
	gc_assign(m_arr,t_vals);
	m_arrLength=m_arr.Length();
	m_length=m_arrLength;
	return this;
}
c_FlashArray* c_FlashArray::m_new3(){
	return this;
}
void c_FlashArray::p_Set3(int t_index,c_b2ContactRegister* t_item){
	if(t_index>=m_arrLength){
		m_arrLength=t_index+100;
		gc_assign(m_arr,m_arr.Resize(m_arrLength));
	}
	gc_assign(m_arr[t_index],t_item);
	if(t_index>=m_length){
		m_length=t_index+1;
	}
}
c_b2ContactRegister* c_FlashArray::p_Get(int t_index){
	if(t_index>=0 && m_length>t_index){
		return m_arr[t_index];
	}else{
		return 0;
	}
}
void c_FlashArray::mark(){
	Object::mark();
	gc_mark_q(m_arr);
}
c_FlashArray2::c_FlashArray2(){
	m_length=0;
	m_arrLength=100;
	m_arr=Array<c_FlashArray* >(100);
}
int c_FlashArray2::p_Length(){
	return m_length;
}
void c_FlashArray2::p_Length2(int t_value){
	m_length=t_value;
	if(m_length>m_arrLength){
		m_arrLength=m_length;
		gc_assign(m_arr,m_arr.Resize(m_length));
	}
}
c_FlashArray2* c_FlashArray2::m_new(int t_length){
	p_Length2(t_length);
	return this;
}
c_FlashArray2* c_FlashArray2::m_new2(Array<c_FlashArray* > t_vals){
	gc_assign(m_arr,t_vals);
	m_arrLength=m_arr.Length();
	m_length=m_arrLength;
	return this;
}
c_FlashArray2* c_FlashArray2::m_new3(){
	return this;
}
void c_FlashArray2::p_Set4(int t_index,c_FlashArray* t_item){
	if(t_index>=m_arrLength){
		m_arrLength=t_index+100;
		gc_assign(m_arr,m_arr.Resize(m_arrLength));
	}
	gc_assign(m_arr[t_index],t_item);
	if(t_index>=m_length){
		m_length=t_index+1;
	}
}
c_FlashArray* c_FlashArray2::p_Get(int t_index){
	if(t_index>=0 && m_length>t_index){
		return m_arr[t_index];
	}else{
		return 0;
	}
}
void c_FlashArray2::mark(){
	Object::mark();
	gc_mark_q(m_arr);
}
c_b2Shape::c_b2Shape(){
	m_m_type=0;
	m_m_radius=FLOAT(.0);
}
int c_b2Shape::p_GetType(){
	return m_m_type;
}
c_b2Shape* c_b2Shape::m_new(){
	m_m_type=-1;
	m_m_radius=FLOAT(0.005);
	return this;
}
c_b2Shape* c_b2Shape::p_Copy(){
	return 0;
}
void c_b2Shape::p_ComputeAABB(c_b2AABB* t_aabb,c_b2Transform* t_xf){
}
void c_b2Shape::p_ComputeMass(c_b2MassData* t_massData,Float t_density){
}
bool c_b2Shape::m_TestOverlap(c_b2Shape* t_shape1,c_b2Transform* t_transform1,c_b2Shape* t_shape2,c_b2Transform* t_transform2){
	c_b2DistanceInput* t_input=(new c_b2DistanceInput)->m_new();
	gc_assign(t_input->m_proxyA,(new c_b2DistanceProxy)->m_new());
	t_input->m_proxyA->p_Set5(t_shape1);
	gc_assign(t_input->m_proxyB,(new c_b2DistanceProxy)->m_new());
	t_input->m_proxyB->p_Set5(t_shape2);
	gc_assign(t_input->m_transformA,t_transform1);
	gc_assign(t_input->m_transformB,t_transform2);
	t_input->m_useRadii=true;
	c_b2SimplexCache* t_simplexCache=(new c_b2SimplexCache)->m_new();
	t_simplexCache->m_count=0;
	c_b2DistanceOutput* t_output=(new c_b2DistanceOutput)->m_new();
	c_b2Distance::m_Distance(t_output,t_simplexCache,t_input);
	return t_output->m_distance<FLOAT(1.0000000000000002e-014);
}
void c_b2Shape::p_Set5(c_b2Shape* t_other){
	m_m_radius=t_other->m_m_radius;
}
void c_b2Shape::mark(){
	Object::mark();
}
c_ContactTypeFactory::c_ContactTypeFactory(){
}
c_ContactTypeFactory* c_ContactTypeFactory::m_new(){
	return this;
}
void c_ContactTypeFactory::mark(){
	Object::mark();
}
c_CircleContactTypeFactory::c_CircleContactTypeFactory(){
}
c_CircleContactTypeFactory* c_CircleContactTypeFactory::m_new(){
	c_ContactTypeFactory::m_new();
	return this;
}
void c_CircleContactTypeFactory::p_Destroy2(c_b2Contact* t_contact,Object* t_allocator){
}
c_b2Contact* c_CircleContactTypeFactory::p_Create2(Object* t_allocator){
	return ((new c_b2CircleContact)->m_new());
}
void c_CircleContactTypeFactory::mark(){
	c_ContactTypeFactory::mark();
}
c_PolyAndCircleContactTypeFactory::c_PolyAndCircleContactTypeFactory(){
}
c_PolyAndCircleContactTypeFactory* c_PolyAndCircleContactTypeFactory::m_new(){
	c_ContactTypeFactory::m_new();
	return this;
}
void c_PolyAndCircleContactTypeFactory::p_Destroy2(c_b2Contact* t_contact,Object* t_allocator){
}
c_b2Contact* c_PolyAndCircleContactTypeFactory::p_Create2(Object* t_allocator){
	return ((new c_b2PolyAndCircleContact)->m_new());
}
void c_PolyAndCircleContactTypeFactory::mark(){
	c_ContactTypeFactory::mark();
}
c_PolygonContactTypeFactory::c_PolygonContactTypeFactory(){
}
c_PolygonContactTypeFactory* c_PolygonContactTypeFactory::m_new(){
	c_ContactTypeFactory::m_new();
	return this;
}
void c_PolygonContactTypeFactory::p_Destroy2(c_b2Contact* t_contact,Object* t_allocator){
}
c_b2Contact* c_PolygonContactTypeFactory::p_Create2(Object* t_allocator){
	return ((new c_b2PolygonContact)->m_new());
}
void c_PolygonContactTypeFactory::mark(){
	c_ContactTypeFactory::mark();
}
c_EdgeAndCircleContactTypeFactory::c_EdgeAndCircleContactTypeFactory(){
}
c_EdgeAndCircleContactTypeFactory* c_EdgeAndCircleContactTypeFactory::m_new(){
	c_ContactTypeFactory::m_new();
	return this;
}
void c_EdgeAndCircleContactTypeFactory::p_Destroy2(c_b2Contact* t_contact,Object* t_allocator){
}
c_b2Contact* c_EdgeAndCircleContactTypeFactory::p_Create2(Object* t_allocator){
	return ((new c_b2EdgeAndCircleContact)->m_new());
}
void c_EdgeAndCircleContactTypeFactory::mark(){
	c_ContactTypeFactory::mark();
}
c_PolyAndEdgeContactTypeFactory::c_PolyAndEdgeContactTypeFactory(){
}
c_PolyAndEdgeContactTypeFactory* c_PolyAndEdgeContactTypeFactory::m_new(){
	c_ContactTypeFactory::m_new();
	return this;
}
void c_PolyAndEdgeContactTypeFactory::p_Destroy2(c_b2Contact* t_contact,Object* t_allocator){
}
c_b2Contact* c_PolyAndEdgeContactTypeFactory::p_Create2(Object* t_allocator){
	return ((new c_b2PolyAndEdgeContact)->m_new());
}
void c_PolyAndEdgeContactTypeFactory::mark(){
	c_ContactTypeFactory::mark();
}
c_IBroadPhase::c_IBroadPhase(){
}
c_IBroadPhase* c_IBroadPhase::m_new(){
	return this;
}
void c_IBroadPhase::mark(){
	Object::mark();
}
c_b2DynamicTreeBroadPhase::c_b2DynamicTreeBroadPhase(){
	m_m_tree=(new c_b2DynamicTree)->m_new();
	m_m_proxyCount=0;
	m_m_moveBuffer=(new c_FlashArray3)->m_new3();
	m_dtQueryCallBack=(new c_DTQueryCallback)->m_new();
}
c_b2DynamicTreeBroadPhase* c_b2DynamicTreeBroadPhase::m_new(){
	c_IBroadPhase::m_new();
	return this;
}
void c_b2DynamicTreeBroadPhase::p_BufferMove(c_b2DynamicTreeNode* t_proxy){
	m_m_moveBuffer->p_Set12(m_m_moveBuffer->p_Length(),t_proxy);
}
Object* c_b2DynamicTreeBroadPhase::p_CreateProxy(c_b2AABB* t_aabb,Object* t_userData){
	c_b2DynamicTreeNode* t_proxy=m_m_tree->p_CreateProxy(t_aabb,t_userData);
	m_m_proxyCount+=1;
	p_BufferMove(t_proxy);
	return (t_proxy);
}
void c_b2DynamicTreeBroadPhase::p_UnBufferMove(c_b2DynamicTreeNode* t_proxy){
	int t_i=m_m_moveBuffer->p_IndexOf(t_proxy);
	m_m_moveBuffer->p_Splice3(t_i,1);
}
void c_b2DynamicTreeBroadPhase::p_DestroyProxy(Object* t_proxy){
	p_UnBufferMove(dynamic_cast<c_b2DynamicTreeNode*>(t_proxy));
	m_m_proxyCount-=1;
	m_m_tree->p_DestroyProxy3(dynamic_cast<c_b2DynamicTreeNode*>(t_proxy));
}
void c_b2DynamicTreeBroadPhase::p_MoveProxy(Object* t_proxy,c_b2AABB* t_aabb,c_b2Vec2* t_displacement){
	bool t_buffer=m_m_tree->p_MoveProxy2(dynamic_cast<c_b2DynamicTreeNode*>(t_proxy),t_aabb,t_displacement);
	if(t_buffer){
		p_BufferMove(dynamic_cast<c_b2DynamicTreeNode*>(t_proxy));
	}
}
bool c_b2DynamicTreeBroadPhase::p_TestOverlap(Object* t_proxyA,Object* t_proxyB){
	c_b2AABB* t_aabbA=m_m_tree->p_GetFatAABB2(dynamic_cast<c_b2DynamicTreeNode*>(t_proxyA));
	c_b2AABB* t_aabbB=m_m_tree->p_GetFatAABB2(dynamic_cast<c_b2DynamicTreeNode*>(t_proxyB));
	return t_aabbA->p_TestOverlap2(t_aabbB);
}
c_b2AABB* c_b2DynamicTreeBroadPhase::p_GetFatAABB(Object* t_proxy){
	return m_m_tree->p_GetFatAABB2(dynamic_cast<c_b2DynamicTreeNode*>(t_proxy));
}
void c_b2DynamicTreeBroadPhase::p_UpdatePairs(c_UpdatePairsCallback* t_callback){
	m_dtQueryCallBack->m_m_pairCount=0;
	Array<c_b2DynamicTreeNode* > t_nodes=m_m_moveBuffer->p_BackingArray();
	for(int t_i=0;t_i<m_m_moveBuffer->p_Length();t_i=t_i+1){
		c_b2DynamicTreeNode* t_queryProxy=t_nodes[t_i];
		c_b2AABB* t_fatAABB=m_m_tree->p_GetFatAABB2(t_queryProxy);
		gc_assign(m_dtQueryCallBack->m_queryProxy,t_queryProxy);
		m_m_tree->p_Query((m_dtQueryCallBack),t_fatAABB);
	}
	m_m_moveBuffer->p_Length2(0);
	int t_i2=0;
	while(t_i2<m_dtQueryCallBack->m_m_pairCount){
		c_b2DynamicTreePair* t_primaryPair=m_dtQueryCallBack->m_m_pairBuffer->p_Get(t_i2);
		Object* t_userDataA=m_m_tree->p_GetUserData(t_primaryPair->m_proxyA);
		Object* t_userDataB=m_m_tree->p_GetUserData(t_primaryPair->m_proxyB);
		t_callback->p_Callback(t_userDataA,t_userDataB);
		t_i2+=1;
		while(t_i2<m_dtQueryCallBack->m_m_pairCount){
			c_b2DynamicTreePair* t_pair=m_dtQueryCallBack->m_m_pairBuffer->p_Get(t_i2);
			if(t_pair->m_proxyA!=t_primaryPair->m_proxyA || t_pair->m_proxyB!=t_primaryPair->m_proxyB){
				break;
			}
			t_i2+=1;
		}
	}
}
void c_b2DynamicTreeBroadPhase::mark(){
	c_IBroadPhase::mark();
	gc_mark_q(m_m_tree);
	gc_mark_q(m_m_moveBuffer);
	gc_mark_q(m_dtQueryCallBack);
}
c_UpdatePairsCallback::c_UpdatePairsCallback(){
}
c_UpdatePairsCallback* c_UpdatePairsCallback::m_new(){
	return this;
}
void c_UpdatePairsCallback::mark(){
	Object::mark();
}
c_CMUpdatePairsCallback::c_CMUpdatePairsCallback(){
	m_cm=0;
}
c_CMUpdatePairsCallback* c_CMUpdatePairsCallback::m_new(c_b2ContactManager* t_cm){
	c_UpdatePairsCallback::m_new();
	gc_assign(this->m_cm,t_cm);
	return this;
}
c_CMUpdatePairsCallback* c_CMUpdatePairsCallback::m_new2(){
	c_UpdatePairsCallback::m_new();
	return this;
}
void c_CMUpdatePairsCallback::p_Callback(Object* t_a,Object* t_b){
	m_cm->p_AddPair(t_a,t_b);
}
void c_CMUpdatePairsCallback::mark(){
	c_UpdatePairsCallback::mark();
	gc_mark_q(m_cm);
}
c_b2BodyDef::c_b2BodyDef(){
	m_userData=0;
	m_position=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_angle=FLOAT(.0);
	m_linearVelocity=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_angularVelocity=FLOAT(.0);
	m_linearDamping=FLOAT(.0);
	m_angularDamping=FLOAT(.0);
	m_allowSleep=false;
	m_awake=false;
	m_fixedRotation=false;
	m_bullet=false;
	m_type=0;
	m_active=false;
	m_inertiaScale=FLOAT(.0);
}
c_b2BodyDef* c_b2BodyDef::m_new(){
	m_userData=0;
	m_position->p_Set2(FLOAT(0.0),FLOAT(0.0));
	m_angle=FLOAT(0.0);
	m_linearVelocity->p_Set2(FLOAT(0.0),FLOAT(0.0));
	m_angularVelocity=FLOAT(0.0);
	m_linearDamping=FLOAT(0.0);
	m_angularDamping=FLOAT(0.0);
	m_allowSleep=true;
	m_awake=true;
	m_fixedRotation=false;
	m_bullet=false;
	m_type=0;
	m_active=true;
	m_inertiaScale=FLOAT(1.0);
	return this;
}
void c_b2BodyDef::mark(){
	Object::mark();
	gc_mark_q(m_userData);
	gc_mark_q(m_position);
	gc_mark_q(m_linearVelocity);
}
c_b2Transform::c_b2Transform(){
	m_position=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_R=(new c_b2Mat22)->m_new();
}
c_b2Transform* c_b2Transform::m_new(c_b2Vec2* t_pos,c_b2Mat22* t_r){
	if((t_pos)!=0){
		m_position->p_SetV(t_pos);
		m_R->p_SetM(t_r);
	}
	return this;
}
void c_b2Transform::mark(){
	Object::mark();
	gc_mark_q(m_position);
	gc_mark_q(m_R);
}
c_b2Mat22::c_b2Mat22(){
	m_col2=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_col1=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
}
c_b2Mat22* c_b2Mat22::m_new(){
	m_col2->m_y=FLOAT(1.0);
	m_col1->m_x=FLOAT(1.0);
	return this;
}
void c_b2Mat22::p_SetM(c_b2Mat22* t_m){
	m_col1->m_x=t_m->m_col1->m_x;
	m_col1->m_y=t_m->m_col1->m_y;
	m_col2->m_x=t_m->m_col2->m_x;
	m_col2->m_y=t_m->m_col2->m_y;
}
void c_b2Mat22::p_Set6(Float t_angle){
	Float t_c=(Float)cos(t_angle);
	Float t_s=(Float)sin(t_angle);
	m_col1->m_x=t_c;
	m_col2->m_x=-t_s;
	m_col1->m_y=t_s;
	m_col2->m_y=t_c;
}
c_b2Mat22* c_b2Mat22::p_GetInverse(c_b2Mat22* t_out){
	Float t_a=m_col1->m_x;
	Float t_b=m_col2->m_x;
	Float t_c=m_col1->m_y;
	Float t_d=m_col2->m_y;
	Float t_det=t_a*t_d-t_b*t_c;
	if(t_det!=FLOAT(0.0)){
		t_det=FLOAT(1.0)/t_det;
	}
	t_out->m_col1->m_x=t_det*t_d;
	t_out->m_col2->m_x=-t_det*t_b;
	t_out->m_col1->m_y=-t_det*t_c;
	t_out->m_col2->m_y=t_det*t_a;
	return t_out;
}
void c_b2Mat22::mark(){
	Object::mark();
	gc_mark_q(m_col2);
	gc_mark_q(m_col1);
}
c_b2Sweep::c_b2Sweep(){
	m_localCenter=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_t0=FLOAT(.0);
	m_a0=FLOAT(.0);
	m_a=FLOAT(.0);
	m_c=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_c0=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
}
c_b2Sweep* c_b2Sweep::m_new(){
	return this;
}
void c_b2Sweep::p_Advance(Float t_t){
	if(m_t0<t_t && FLOAT(1.0)-m_t0>FLOAT(1e-15)){
		Float t_alpha=(t_t-m_t0)/(FLOAT(1.0)-m_t0);
		m_c0->m_x=(FLOAT(1.0)-t_alpha)*m_c0->m_x+t_alpha*m_c->m_x;
		m_c0->m_y=(FLOAT(1.0)-t_alpha)*m_c0->m_y+t_alpha*m_c->m_y;
		m_a0=(FLOAT(1.0)-t_alpha)*m_a0+t_alpha*m_a;
		m_t0=t_t;
	}
}
void c_b2Sweep::p_GetTransform2(c_b2Transform* t_xf,Float t_alpha){
	t_xf->m_position->m_x=(FLOAT(1.0)-t_alpha)*m_c0->m_x+t_alpha*m_c->m_x;
	t_xf->m_position->m_y=(FLOAT(1.0)-t_alpha)*m_c0->m_y+t_alpha*m_c->m_y;
	Float t_angle=(FLOAT(1.0)-t_alpha)*m_a0+t_alpha*m_a;
	t_xf->m_R->p_Set6(t_angle);
	c_b2Mat22* t_tMat=t_xf->m_R;
	t_xf->m_position->m_x-=t_tMat->m_col1->m_x*m_localCenter->m_x+t_tMat->m_col2->m_x*m_localCenter->m_y;
	t_xf->m_position->m_y-=t_tMat->m_col1->m_y*m_localCenter->m_x+t_tMat->m_col2->m_y*m_localCenter->m_y;
}
void c_b2Sweep::p_Set7(c_b2Sweep* t_other){
	m_localCenter->p_SetV(t_other->m_localCenter);
	m_c0->p_SetV(t_other->m_c0);
	m_c->p_SetV(t_other->m_c);
	m_a0=t_other->m_a0;
	m_a=t_other->m_a;
	m_t0=t_other->m_t0;
}
void c_b2Sweep::mark(){
	Object::mark();
	gc_mark_q(m_localCenter);
	gc_mark_q(m_c);
	gc_mark_q(m_c0);
}
c_b2JointEdge::c_b2JointEdge(){
	m_nextItem=0;
	m_joint=0;
	m_prevItem=0;
	m_other=0;
}
c_b2JointEdge* c_b2JointEdge::m_new(){
	return this;
}
void c_b2JointEdge::mark(){
	Object::mark();
	gc_mark_q(m_nextItem);
	gc_mark_q(m_joint);
	gc_mark_q(m_prevItem);
	gc_mark_q(m_other);
}
c_b2ControllerEdge::c_b2ControllerEdge(){
	m_nextController=0;
	m_controller=0;
	m_prevBody=0;
	m_nextBody=0;
	m_prevController=0;
}
void c_b2ControllerEdge::mark(){
	Object::mark();
	gc_mark_q(m_nextController);
	gc_mark_q(m_controller);
	gc_mark_q(m_prevBody);
	gc_mark_q(m_nextBody);
	gc_mark_q(m_prevController);
}
c_b2ContactEdge::c_b2ContactEdge(){
	m_other=0;
	m_contact=0;
	m_nextItem=0;
	m_prevItem=0;
}
c_b2ContactEdge* c_b2ContactEdge::m_new(){
	return this;
}
void c_b2ContactEdge::mark(){
	Object::mark();
	gc_mark_q(m_other);
	gc_mark_q(m_contact);
	gc_mark_q(m_nextItem);
	gc_mark_q(m_prevItem);
}
c_b2Fixture::c_b2Fixture(){
	m_m_body=0;
	m_m_shape=0;
	m_m_next=0;
	m_m_proxy=0;
	m_m_aabb=0;
	m_m_userData=0;
	m_m_density=FLOAT(.0);
	m_m_friction=FLOAT(.0);
	m_m_restitution=FLOAT(.0);
	m_m_filter=(new c_b2FilterData)->m_new();
	m_m_isSensor=false;
}
c_b2Body* c_b2Fixture::p_GetBody(){
	return m_m_body;
}
int c_b2Fixture::p_GetType(){
	return m_m_shape->p_GetType();
}
void c_b2Fixture::p_DestroyProxy2(c_IBroadPhase* t_broadPhase){
	if(m_m_proxy==0){
		return;
	}
	t_broadPhase->p_DestroyProxy(m_m_proxy);
	m_m_proxy=0;
}
void c_b2Fixture::p_Destroy3(){
	m_m_shape=0;
}
c_b2Fixture* c_b2Fixture::m_new(){
	gc_assign(m_m_aabb,(new c_b2AABB)->m_new());
	m_m_userData=0;
	m_m_body=0;
	m_m_next=0;
	m_m_shape=0;
	m_m_density=FLOAT(0.0);
	m_m_friction=FLOAT(0.0);
	m_m_restitution=FLOAT(0.0);
	return this;
}
void c_b2Fixture::p_Create3(c_b2Body* t_body,c_b2Transform* t_xf,c_b2FixtureDef* t_def){
	gc_assign(m_m_userData,t_def->m_userData);
	m_m_friction=t_def->m_friction;
	m_m_restitution=t_def->m_restitution;
	gc_assign(m_m_body,t_body);
	m_m_next=0;
	gc_assign(m_m_filter,t_def->m_filter->p_Copy());
	m_m_isSensor=t_def->m_isSensor;
	gc_assign(m_m_shape,t_def->m_shape->p_Copy());
	m_m_density=t_def->m_density;
}
void c_b2Fixture::p_CreateProxy2(c_IBroadPhase* t_broadPhase,c_b2Transform* t_xf){
	m_m_shape->p_ComputeAABB(m_m_aabb,t_xf);
	gc_assign(m_m_proxy,t_broadPhase->p_CreateProxy(m_m_aabb,(this)));
}
c_b2MassData* c_b2Fixture::p_GetMassData(c_b2MassData* t_massData){
	if(t_massData==0){
		t_massData=(new c_b2MassData)->m_new();
	}
	m_m_shape->p_ComputeMass(t_massData,m_m_density);
	return t_massData;
}
c_b2AABB* c_b2Fixture::m_tmpAABB1;
c_b2AABB* c_b2Fixture::m_tmpAABB2;
c_b2Vec2* c_b2Fixture::m_tmpVec;
void c_b2Fixture::p_Synchronize(c_IBroadPhase* t_broadPhase,c_b2Transform* t_transform1,c_b2Transform* t_transform2){
	if(!((m_m_proxy)!=0)){
		return;
	}
	m_m_shape->p_ComputeAABB(m_tmpAABB1,t_transform1);
	m_m_shape->p_ComputeAABB(m_tmpAABB2,t_transform2);
	m_m_aabb->p_Combine(m_tmpAABB1,m_tmpAABB2);
	c_b2Math::m_SubtractVV(t_transform2->m_position,t_transform1->m_position,m_tmpVec);
	t_broadPhase->p_MoveProxy(m_m_proxy,m_m_aabb,m_tmpVec);
}
c_b2FilterData* c_b2Fixture::p_GetFilterData(){
	return m_m_filter->p_Copy();
}
c_b2Shape* c_b2Fixture::p_GetShape(){
	return m_m_shape;
}
Float c_b2Fixture::p_GetFriction(){
	return m_m_friction;
}
Float c_b2Fixture::p_GetRestitution(){
	return m_m_restitution;
}
c_b2AABB* c_b2Fixture::p_GetAABB(){
	return m_m_aabb;
}
c_b2Fixture* c_b2Fixture::p_GetNext(){
	return m_m_next;
}
bool c_b2Fixture::p_IsSensor(){
	return m_m_isSensor;
}
void c_b2Fixture::mark(){
	Object::mark();
	gc_mark_q(m_m_body);
	gc_mark_q(m_m_shape);
	gc_mark_q(m_m_next);
	gc_mark_q(m_m_proxy);
	gc_mark_q(m_m_aabb);
	gc_mark_q(m_m_userData);
	gc_mark_q(m_m_filter);
}
c_Entity::c_Entity(){
	m_body=0;
	m_bodyShape=0;
	m_bodyDef=0;
	m_bodyShapeCircle=0;
	m_fixtureDef=0;
	m_img=0;
	m_world=0;
	m_scale=FLOAT(.0);
	m_frame=0;
}
c_Entity* c_Entity::m_new(){
	return this;
}
void c_Entity::p_CreateMultiPolygon2(c_b2World* t_world,Float t_x,Float t_y,c_List3* t_polygonList,Float t_scale,bool t_static,Float t_friction){
	gc_assign(this->m_bodyDef,(new c_b2BodyDef)->m_new());
	if(t_static==true){
		this->m_bodyDef->m_type=0;
	}else{
		this->m_bodyDef->m_type=2;
	}
	this->m_bodyDef->m_position->p_Set2(t_x,t_y);
	gc_assign(this->m_body,t_world->p_CreateBody(this->m_bodyDef));
	c_Enumerator2* t_=t_polygonList->p_ObjectEnumerator();
	while(t_->p_HasNext()){
		c_Polygon* t_p=t_->p_NextObject();
		gc_assign(this->m_bodyShape,(new c_b2PolygonShape)->m_new());
		this->m_bodyShape->p_SetAsArray(t_p->m_vertices,Float(t_p->m_count));
		gc_assign(this->m_fixtureDef,(new c_b2FixtureDef)->m_new());
		gc_assign(this->m_fixtureDef->m_shape,(this->m_bodyShape));
		this->m_fixtureDef->m_density=FLOAT(1.0);
		this->m_fixtureDef->m_friction=t_friction;
		this->m_fixtureDef->m_restitution=FLOAT(0.3);
		this->m_bodyDef->m_position->p_Set2(t_x,t_y);
		this->m_body->p_CreateFixture(this->m_fixtureDef);
	}
	this->m_bodyDef->m_position->p_Set2(t_x,t_y);
	gc_assign(this->m_world,t_world);
	this->m_bodyDef->m_allowSleep=true;
	this->m_bodyDef->m_awake=true;
	this->m_scale=t_scale;
}
void c_Entity::p_CreateBox(c_b2World* t_world,Float t_x,Float t_y,Float t_width,Float t_height,Float t_scale,Float t_restitution,Float t_density,Float t_friction,bool t_static,bool t_sensor){
	gc_assign(this->m_fixtureDef,(new c_b2FixtureDef)->m_new());
	gc_assign(this->m_bodyShape,(new c_b2PolygonShape)->m_new());
	gc_assign(this->m_bodyDef,(new c_b2BodyDef)->m_new());
	if(t_static==true){
		this->m_bodyDef->m_type=0;
	}else{
		this->m_bodyDef->m_type=2;
	}
	this->m_fixtureDef->m_density=t_density;
	this->m_fixtureDef->m_friction=t_friction;
	this->m_fixtureDef->m_restitution=t_restitution;
	gc_assign(this->m_fixtureDef->m_shape,(this->m_bodyShape));
	this->m_bodyDef->m_position->p_Set2(t_x,t_y);
	if(t_sensor==true){
		this->m_bodyDef->m_allowSleep=false;
		this->m_bodyDef->m_awake=true;
	}else{
		this->m_bodyDef->m_allowSleep=true;
		this->m_bodyDef->m_awake=true;
	}
	this->m_bodyShape->p_SetAsBox(t_width,t_height);
	gc_assign(this->m_body,t_world->p_CreateBody(this->m_bodyDef));
	this->m_body->p_CreateFixture(this->m_fixtureDef);
	gc_assign(this->m_world,t_world);
	this->m_scale=t_scale;
}
void c_Entity::p_CreateImageBox2(c_b2World* t_world,c_Image* t_img,Float t_x,Float t_y,Float t_scale,Float t_restitution,Float t_density,Float t_friction,bool t_static,bool t_sensor){
	gc_assign(this->m_img,t_img);
	this->p_CreateBox(t_world,t_x,t_y,Float(this->m_img->p_Width())/(t_scale*FLOAT(2.0)),Float(t_img->p_Height())/(t_scale*FLOAT(2.0)),t_scale,t_restitution,t_density,t_friction,t_static,t_sensor);
}
void c_Entity::p_Kill(){
	this->m_world->p_DestroyBody(this->m_body);
	this->m_bodyShape=0;
	this->m_bodyDef=0;
	this->m_bodyShapeCircle=0;
	this->m_fixtureDef=0;
	this->m_img=0;
}
int c_Entity::p_RadToDeg(Float t_rad){
	if(t_rad!=FLOAT(0.0)){
		return int(t_rad*FLOAT(180.0)/FLOAT(3.14159265));
	}else{
		return 0;
	}
}
void c_Entity::p_Draw2(Float t_offsetX,Float t_offsetY){
	if(this->m_img!=0){
		Float t_x=this->m_body->p_GetPosition()->m_x*this->m_scale;
		Float t_y=this->m_body->p_GetPosition()->m_y*this->m_scale;
		Float t_r=Float(p_RadToDeg(this->m_body->p_GetAngle())*-1);
		bb_graphics_DrawImage2(this->m_img,t_x,t_y,t_r,FLOAT(1.0),FLOAT(1.0),this->m_frame);
	}
}
void c_Entity::mark(){
	Object::mark();
	gc_mark_q(m_body);
	gc_mark_q(m_bodyShape);
	gc_mark_q(m_bodyDef);
	gc_mark_q(m_bodyShapeCircle);
	gc_mark_q(m_fixtureDef);
	gc_mark_q(m_img);
	gc_mark_q(m_world);
}
c_List::c_List(){
	m__head=((new c_HeadNode)->m_new());
}
c_List* c_List::m_new(){
	return this;
}
c_Node2* c_List::p_AddLast(c_Entity* t_data){
	return (new c_Node2)->m_new(m__head,m__head->m__pred,t_data);
}
c_List* c_List::m_new2(Array<c_Entity* > t_data){
	Array<c_Entity* > t_=t_data;
	int t_2=0;
	while(t_2<t_.Length()){
		c_Entity* t_t=t_[t_2];
		t_2=t_2+1;
		p_AddLast(t_t);
	}
	return this;
}
c_Enumerator* c_List::p_ObjectEnumerator(){
	return (new c_Enumerator)->m_new(this);
}
int c_List::p_Count(){
	int t_n=0;
	c_Node2* t_node=m__head->m__succ;
	while(t_node!=m__head){
		t_node=t_node->m__succ;
		t_n+=1;
	}
	return t_n;
}
void c_List::mark(){
	Object::mark();
	gc_mark_q(m__head);
}
c_Node2::c_Node2(){
	m__succ=0;
	m__pred=0;
	m__data=0;
}
c_Node2* c_Node2::m_new(c_Node2* t_succ,c_Node2* t_pred,c_Entity* t_data){
	gc_assign(m__succ,t_succ);
	gc_assign(m__pred,t_pred);
	gc_assign(m__succ->m__pred,this);
	gc_assign(m__pred->m__succ,this);
	gc_assign(m__data,t_data);
	return this;
}
c_Node2* c_Node2::m_new2(){
	return this;
}
void c_Node2::mark(){
	Object::mark();
	gc_mark_q(m__succ);
	gc_mark_q(m__pred);
	gc_mark_q(m__data);
}
c_HeadNode::c_HeadNode(){
}
c_HeadNode* c_HeadNode::m_new(){
	c_Node2::m_new2();
	gc_assign(m__succ,(this));
	gc_assign(m__pred,(this));
	return this;
}
void c_HeadNode::mark(){
	c_Node2::mark();
}
c_Barrier::c_Barrier(){
	m_ent=0;
	m_x=FLOAT(.0);
	m_y=FLOAT(.0);
	m_startX=FLOAT(.0);
	m_startY=FLOAT(.0);
	m_endX=FLOAT(.0);
	m_endY=FLOAT(.0);
	m_direction=0;
	m_speed=FLOAT(.0);
}
c_Barrier* c_Barrier::m_new(c_Box2D_World* t_world,c_Image* t_img,Float t_x,Float t_y,Float t_sX,Float t_sY,Float t_eX,Float t_eY,int t_dir,Float t_speed,bool t_static){
	if(t_dir<4){
		gc_assign(this->m_ent,t_world->p_CreateImageBox(t_img,FLOAT(400.0),FLOAT(476.0),t_static,FLOAT(0.89),FLOAT(1.0),FLOAT(1.0),false));
	}else{
		if(t_dir==4 || t_dir==5){
			gc_assign(this->m_ent,t_world->p_CreateMultiPolygon(FLOAT(960.0),FLOAT(540.0),bb_Rebound_CreateCross1(),t_static,FLOAT(0.9)));
			gc_assign(this->m_ent->m_img,t_img);
			this->m_ent->m_body->p_SetAngle(FLOAT(45.0));
		}else{
			if(t_dir==6){
				gc_assign(this->m_ent,t_world->p_CreateImageBox(t_img,t_x,t_y,false,FLOAT(0.20),FLOAT(4.9999999999999996e+028),FLOAT(0.2),false));
			}
		}
	}
	this->m_x=t_x;
	this->m_y=t_y;
	this->m_startX=t_sX;
	this->m_startY=t_sY;
	this->m_endX=t_eX;
	this->m_endY=t_eY;
	this->m_direction=t_dir;
	this->m_speed=t_speed;
	return this;
}
c_Barrier* c_Barrier::m_new2(){
	return this;
}
void c_Barrier::p_Update(){
	if(this->m_direction==0){
		this->m_y=this->m_y+this->m_speed;
		if(this->m_y>=this->m_endY){
			this->m_direction=1;
		}
	}else{
		if(this->m_direction==1){
			this->m_y=this->m_y-this->m_speed;
			if(this->m_y<=this->m_startY){
				this->m_direction=0;
			}
		}
	}
	if(this->m_direction==2){
		this->m_x=this->m_x+this->m_speed;
		if(this->m_x>=this->m_endX){
			this->m_direction=3;
		}
	}else{
		if(this->m_direction==3){
			this->m_x=this->m_x-this->m_speed;
			if(this->m_x<=this->m_startX){
				this->m_direction=2;
			}
		}
	}
	if(this->m_direction==4){
		Float t_a=this->m_ent->m_body->p_GetAngle();
		this->m_ent->m_body->p_SetAngle(t_a+m_speed);
	}
	if(this->m_direction==5){
		Float t_a2=this->m_ent->m_body->p_GetAngle();
		this->m_ent->m_body->p_SetAngle(t_a2-m_speed);
	}
	if(this->m_direction==6){
	}
	if(this->m_ent->m_bodyDef->m_type==0){
		this->m_ent->m_body->p_SetPosition((new c_b2Vec2)->m_new(this->m_x/FLOAT(64.0),this->m_y/FLOAT(64.0)));
	}
}
void c_Barrier::mark(){
	Object::mark();
	gc_mark_q(m_ent);
}
c_List2::c_List2(){
	m__head=((new c_HeadNode2)->m_new());
}
c_List2* c_List2::m_new(){
	return this;
}
c_Node3* c_List2::p_AddLast2(c_Barrier* t_data){
	return (new c_Node3)->m_new(m__head,m__head->m__pred,t_data);
}
c_List2* c_List2::m_new2(Array<c_Barrier* > t_data){
	Array<c_Barrier* > t_=t_data;
	int t_2=0;
	while(t_2<t_.Length()){
		c_Barrier* t_t=t_[t_2];
		t_2=t_2+1;
		p_AddLast2(t_t);
	}
	return this;
}
c_Enumerator3* c_List2::p_ObjectEnumerator(){
	return (new c_Enumerator3)->m_new(this);
}
int c_List2::p_Clear(){
	gc_assign(m__head->m__succ,m__head);
	gc_assign(m__head->m__pred,m__head);
	return 0;
}
void c_List2::mark(){
	Object::mark();
	gc_mark_q(m__head);
}
c_Node3::c_Node3(){
	m__succ=0;
	m__pred=0;
	m__data=0;
}
c_Node3* c_Node3::m_new(c_Node3* t_succ,c_Node3* t_pred,c_Barrier* t_data){
	gc_assign(m__succ,t_succ);
	gc_assign(m__pred,t_pred);
	gc_assign(m__succ->m__pred,this);
	gc_assign(m__pred->m__succ,this);
	gc_assign(m__data,t_data);
	return this;
}
c_Node3* c_Node3::m_new2(){
	return this;
}
void c_Node3::mark(){
	Object::mark();
	gc_mark_q(m__succ);
	gc_mark_q(m__pred);
	gc_mark_q(m__data);
}
c_HeadNode2::c_HeadNode2(){
}
c_HeadNode2* c_HeadNode2::m_new(){
	c_Node3::m_new2();
	gc_assign(m__succ,(this));
	gc_assign(m__pred,(this));
	return this;
}
void c_HeadNode2::mark(){
	c_Node3::mark();
}
Float bb_input_TouchX(int t_index){
	return bb_input_device->p_TouchX(t_index);
}
Float bb_autofit_VDeviceWidth(){
	return c_VirtualDisplay::m_Display->m_vwidth;
}
Float bb_autofit_VTouchX(int t_index,bool t_limit){
	return c_VirtualDisplay::m_Display->p_VTouchX(t_index,t_limit);
}
Float bb_input_TouchY(int t_index){
	return bb_input_device->p_TouchY(t_index);
}
Float bb_autofit_VDeviceHeight(){
	return c_VirtualDisplay::m_Display->m_vheight;
}
Float bb_autofit_VTouchY(int t_index,bool t_limit){
	return c_VirtualDisplay::m_Display->p_VTouchY(t_index,t_limit);
}
c_GameData::c_GameData(){
	m_stage=Array<c_StageData* >(3);
}
c_GameData* c_GameData::m_new(){
	for(int t_i=0;t_i<=2;t_i=t_i+1){
		gc_assign(this->m_stage[t_i],(new c_StageData)->m_new(t_i,true));
	}
	return this;
}
String c_GameData::p_SaveString(int t_progress,int t_music){
	String t_s=String();
	for(int t_i=0;t_i<=2;t_i=t_i+1){
		t_s=t_s+String(this->m_stage[t_i]->m_ID)+String(L",",1);
		if(this->m_stage[t_i]->m_unlocked==true){
			t_s=t_s+String(L"1,",2);
		}else{
			t_s=t_s+String(L"0,",2);
		}
		for(int t_l=0;t_l<=7;t_l=t_l+1){
			t_s=t_s+String(this->m_stage[t_i]->m_level[t_l]->m_ID)+String(L",",1);
			if(this->m_stage[t_i]->m_level[t_l]->m_unlocked==true){
				t_s=t_s+String(L"1,",2);
			}else{
				t_s=t_s+String(L"0,",2);
			}
			t_s=t_s+String(this->m_stage[t_i]->m_level[t_l]->m_starsEarned)+String(L",",1);
			t_s=t_s+String(this->m_stage[t_i]->m_level[t_l]->m_bestTime)+String(L",",1);
		}
	}
	t_s=t_s+String(bb_Rebound_currentVersionCode)+String(L",",1);
	t_s=t_s+String(t_progress)+String(L",",1);
	t_s=t_s+String(t_music);
	return t_s;
}
void c_GameData::p_LoadString(String t_st){
	Array<String > t_s2=t_st.Split(String(L",",1));
	int t_i=0;
	int t_s=0;
	for(int t_s3=0;t_s3<=2;t_s3=t_s3+1){
		this->m_stage[t_s3]->m_ID=(t_s2[t_i]).ToInt();
		t_i=t_i+1;
		if(t_s2[t_i]==String(L"1",1)){
			this->m_stage[t_s3]->m_unlocked=true;
		}else{
			this->m_stage[t_s3]->m_unlocked=false;
		}
		t_i=t_i+1;
		for(int t_l=0;t_l<=7;t_l=t_l+1){
			this->m_stage[t_s3]->m_level[t_l]->m_ID=(t_s2[t_i]).ToInt();
			t_i=t_i+1;
			if(t_s2[t_i]==String(L"1",1)){
				this->m_stage[t_s3]->m_level[t_l]->m_unlocked=true;
			}else{
				this->m_stage[t_s3]->m_level[t_l]->m_unlocked=false;
			}
			t_i=t_i+1;
			this->m_stage[t_s3]->m_level[t_l]->m_starsEarned=(t_s2[t_i]).ToInt();
			t_i=t_i+1;
			this->m_stage[t_s3]->m_level[t_l]->m_bestTime=(t_s2[t_i]).ToFloat();
			t_i=t_i+1;
		}
	}
}
void c_GameData::p_CompleteLevel(int t_stage,int t_level,Float t_time){
	int t_s=t_stage-1;
	int t_l=t_level-1;
	if(t_l==7){
		if(t_s<2){
			this->m_stage[t_s+1]->m_level[0]->m_unlocked=true;
		}
	}else{
		this->m_stage[t_s]->m_level[t_l+1]->m_unlocked=true;
	}
	if(this->m_stage[t_s]->m_level[t_l]->m_bestTime>t_time){
		this->m_stage[t_s]->m_level[t_l]->m_bestTime=t_time;
	}
	int t_tempStars=bb_Rebound_AssignStars(t_stage,t_level,t_time);
	if(this->m_stage[t_s]->m_level[t_l]->m_starsEarned<t_tempStars){
		this->m_stage[t_s]->m_level[t_l]->m_starsEarned=t_tempStars;
	}
}
void c_GameData::mark(){
	Object::mark();
	gc_mark_q(m_stage);
}
c_StageData::c_StageData(){
	m_ID=0;
	m_unlocked=false;
	m_level=Array<c_LevelData* >(8);
}
c_StageData* c_StageData::m_new(int t_id,bool t_unlocked){
	this->m_ID=t_id;
	this->m_unlocked=t_unlocked;
	for(int t_i=0;t_i<=7;t_i=t_i+1){
		gc_assign(this->m_level[t_i],(new c_LevelData)->m_new(t_i,false,0,FLOAT(99999999.00)));
	}
	return this;
}
c_StageData* c_StageData::m_new2(){
	return this;
}
void c_StageData::mark(){
	Object::mark();
	gc_mark_q(m_level);
}
c_LevelData::c_LevelData(){
	m_ID=0;
	m_unlocked=false;
	m_starsEarned=0;
	m_bestTime=FLOAT(.0);
}
c_LevelData* c_LevelData::m_new(int t_id,bool t_unlocked,int t_starsEarned,Float t_bestTime){
	this->m_ID=t_id;
	this->m_unlocked=t_unlocked;
	this->m_starsEarned=t_starsEarned;
	this->m_bestTime=t_bestTime;
	return this;
}
c_LevelData* c_LevelData::m_new2(){
	return this;
}
void c_LevelData::mark(){
	Object::mark();
}
String bb_app_LoadState(){
	return bb_app__game->LoadState();
}
int bb_Rebound_currentVersionCode;
void bb_app_SaveState(String t_state){
	bb_app__game->SaveState(t_state);
}
int bb_Rebound_versionCode;
c_Enumerator::c_Enumerator(){
	m__list=0;
	m__curr=0;
}
c_Enumerator* c_Enumerator::m_new(c_List* t_list){
	gc_assign(m__list,t_list);
	gc_assign(m__curr,t_list->m__head->m__succ);
	return this;
}
c_Enumerator* c_Enumerator::m_new2(){
	return this;
}
bool c_Enumerator::p_HasNext(){
	while(m__curr->m__succ->m__pred!=m__curr){
		gc_assign(m__curr,m__curr->m__succ);
	}
	return m__curr!=m__list->m__head;
}
c_Entity* c_Enumerator::p_NextObject(){
	c_Entity* t_data=m__curr->m__data;
	gc_assign(m__curr,m__curr->m__succ);
	return t_data;
}
void c_Enumerator::mark(){
	Object::mark();
	gc_mark_q(m__list);
	gc_mark_q(m__curr);
}
c_b2Manifold::c_b2Manifold(){
	m_m_points=Array<c_b2ManifoldPoint* >();
	m_m_localPlaneNormal=0;
	m_m_localPoint=0;
	m_m_pointCount=0;
	m_m_type=0;
}
c_b2Manifold* c_b2Manifold::m_new(){
	gc_assign(m_m_points,Array<c_b2ManifoldPoint* >(2));
	for(int t_i=0;t_i<2;t_i=t_i+1){
		gc_assign(m_m_points[t_i],(new c_b2ManifoldPoint)->m_new());
	}
	gc_assign(m_m_localPlaneNormal,(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0)));
	gc_assign(m_m_localPoint,(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0)));
	return this;
}
void c_b2Manifold::mark(){
	Object::mark();
	gc_mark_q(m_m_points);
	gc_mark_q(m_m_localPlaneNormal);
	gc_mark_q(m_m_localPoint);
}
c_b2ManifoldPoint::c_b2ManifoldPoint(){
	m_m_localPoint=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_m_normalImpulse=FLOAT(.0);
	m_m_tangentImpulse=FLOAT(.0);
	m_m_id=(new c_b2ContactID)->m_new();
}
void c_b2ManifoldPoint::p_Reset2(){
	m_m_localPoint->p_SetZero();
	m_m_normalImpulse=FLOAT(0.0);
	m_m_tangentImpulse=FLOAT(0.0);
	m_m_id->p_Key2(0);
}
c_b2ManifoldPoint* c_b2ManifoldPoint::m_new(){
	p_Reset2();
	return this;
}
void c_b2ManifoldPoint::mark(){
	Object::mark();
	gc_mark_q(m_m_localPoint);
	gc_mark_q(m_m_id);
}
c_b2Settings::c_b2Settings(){
}
void c_b2Settings::m_B2Assert(bool t_a){
	if(!t_a){
		bbPrint(String(L"Assertion Failed",16));
	}
}
Float c_b2Settings::m_B2MixFriction(Float t_friction1,Float t_friction2){
	return (Float)sqrt(t_friction1*t_friction2);
}
Float c_b2Settings::m_B2MixRestitution(Float t_restitution1,Float t_restitution2){
	if(t_restitution1>t_restitution2){
		return t_restitution1;
	}else{
		return t_restitution2;
	}
}
void c_b2Settings::mark(){
	Object::mark();
}
c_b2ContactID::c_b2ContactID(){
	m_features=(new c_Features)->m_new();
	m__key=0;
}
c_b2ContactID* c_b2ContactID::m_new(){
	gc_assign(m_features->m__m_id,this);
	return this;
}
int c_b2ContactID::p_Key(){
	return m__key;
}
void c_b2ContactID::p_Key2(int t_value){
	m__key=t_value;
	m_features->m__referenceEdge=m__key&255;
	m_features->m__incidentEdge=(m__key&65280)>>8&255;
	m_features->m__incidentVertex=(m__key&16711680)>>16&255;
	m_features->m__flip=(m__key&-16777216)>>24&255;
}
void c_b2ContactID::p_Set8(c_b2ContactID* t_id){
	p_Key2(t_id->m__key);
}
void c_b2ContactID::mark(){
	Object::mark();
	gc_mark_q(m_features);
}
c_Features::c_Features(){
	m__m_id=0;
	m__referenceEdge=0;
	m__incidentEdge=0;
	m__incidentVertex=0;
	m__flip=0;
}
c_Features* c_Features::m_new(){
	return this;
}
int c_Features::p_ReferenceEdge(){
	return m__referenceEdge;
}
void c_Features::p_ReferenceEdge2(int t_value){
	m__referenceEdge=t_value;
	m__m_id->m__key=m__m_id->m__key&-256|m__referenceEdge&255;
}
int c_Features::p_IncidentEdge(){
	return m__incidentEdge;
}
void c_Features::p_IncidentEdge2(int t_value){
	m__incidentEdge=t_value;
	m__m_id->m__key=m__m_id->m__key&-65281|m__incidentEdge<<8&65280;
}
int c_Features::p_IncidentVertex(){
	return m__incidentVertex;
}
void c_Features::p_IncidentVertex2(int t_value){
	m__incidentVertex=t_value;
	m__m_id->m__key=m__m_id->m__key&-16711681|m__incidentVertex<<16&16711680;
}
int c_Features::p_Flip(){
	return m__flip;
}
void c_Features::p_Flip2(int t_value){
	m__flip=t_value;
	m__m_id->m__key=m__m_id->m__key&16777215|m__flip<<24&-16777216;
}
void c_Features::mark(){
	Object::mark();
	gc_mark_q(m__m_id);
}
c_b2PolygonShape::c_b2PolygonShape(){
	m_m_centroid=0;
	m_m_vertices=Array<c_b2Vec2* >();
	m_m_depths=Array<Float >();
	m_m_normals=Array<c_b2Vec2* >();
	m_m_vertexCount=0;
}
void c_b2PolygonShape::p_Reserve(int t_count){
	if(m_m_vertices.Length()==0){
		gc_assign(m_m_depths,Array<Float >(t_count));
	}
	if(m_m_vertices.Length()<t_count){
		int t_startLength=m_m_vertices.Length();
		gc_assign(m_m_depths,m_m_depths.Resize(t_count));
		gc_assign(m_m_vertices,m_m_vertices.Resize(t_count));
		gc_assign(m_m_normals,m_m_normals.Resize(t_count));
		for(int t_i=t_startLength;t_i<t_count;t_i=t_i+1){
			gc_assign(m_m_vertices[t_i],(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0)));
			gc_assign(m_m_normals[t_i],(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0)));
		}
	}
}
c_b2PolygonShape* c_b2PolygonShape::m_new(){
	c_b2Shape::m_new();
	m_m_type=1;
	gc_assign(m_m_centroid,(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0)));
	p_Reserve(4);
	return this;
}
c_b2Vec2* c_b2PolygonShape::m_ComputeCentroid(Array<c_b2Vec2* > t_vs,int t_count){
	c_b2Vec2* t_c=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	Float t_area=FLOAT(0.0);
	Float t_p1X=FLOAT(0.0);
	Float t_p1Y=FLOAT(0.0);
	Float t_inv3=FLOAT(0.33333333333333331);
	for(int t_i=0;t_i<t_count;t_i=t_i+1){
		c_b2Vec2* t_p2=t_vs[t_i];
		c_b2Vec2* t_p3=t_vs[0];
		if(t_i+1<t_count){
			t_p3=t_vs[t_i+1];
		}
		Float t_e1X=t_p2->m_x-t_p1X;
		Float t_e1Y=t_p2->m_y-t_p1Y;
		Float t_e2X=t_p3->m_x-t_p1X;
		Float t_e2Y=t_p3->m_y-t_p1Y;
		Float t_D=t_e1X*t_e2Y-t_e1Y*t_e2X;
		Float t_triangleArea=FLOAT(0.5)*t_D;
		t_area+=t_triangleArea;
		t_c->m_x+=t_triangleArea*t_inv3*(t_p1X+t_p2->m_x+t_p3->m_x);
		t_c->m_y+=t_triangleArea*t_inv3*(t_p1Y+t_p2->m_y+t_p3->m_y);
	}
	t_c->m_x*=FLOAT(1.0)/t_area;
	t_c->m_y*=FLOAT(1.0)/t_area;
	return t_c;
}
void c_b2PolygonShape::p_SetAsArray(Array<c_b2Vec2* > t_vertices,Float t_vertexCount){
	if(t_vertexCount==FLOAT(0.0)){
		t_vertexCount=Float(t_vertices.Length());
	}
	c_b2Settings::m_B2Assert(FLOAT(2.0)<=t_vertexCount);
	m_m_vertexCount=int(t_vertexCount);
	p_Reserve(int(t_vertexCount));
	int t_i=0;
	for(int t_i2=0;t_i2<m_m_vertexCount;t_i2=t_i2+1){
		m_m_vertices[t_i2]->p_SetV(t_vertices[t_i2]);
	}
	for(int t_i3=0;t_i3<m_m_vertexCount;t_i3=t_i3+1){
		int t_i1=t_i3;
		int t_i22=0;
		if(t_i3+1<m_m_vertexCount){
			t_i22=t_i3+1;
		}
		c_b2Vec2* t_edge=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
		c_b2Math::m_SubtractVV(m_m_vertices[t_i22],m_m_vertices[t_i1],t_edge);
		c_b2Settings::m_B2Assert(t_edge->p_LengthSquared()>FLOAT(1e-15));
		c_b2Math::m_CrossVF(t_edge,FLOAT(1.0),m_m_normals[t_i3]);
		m_m_normals[t_i3]->p_Normalize();
	}
	gc_assign(m_m_centroid,m_ComputeCentroid(m_m_vertices,m_m_vertexCount));
}
void c_b2PolygonShape::p_SetAsBox(Float t_hx,Float t_hy){
	m_m_vertexCount=4;
	p_Reserve(4);
	m_m_vertices[0]->p_Set2(-t_hx,-t_hy);
	m_m_vertices[1]->p_Set2(t_hx,-t_hy);
	m_m_vertices[2]->p_Set2(t_hx,t_hy);
	m_m_vertices[3]->p_Set2(-t_hx,t_hy);
	m_m_normals[0]->p_Set2(FLOAT(0.0),FLOAT(-1.0));
	m_m_normals[1]->p_Set2(FLOAT(1.0),FLOAT(0.0));
	m_m_normals[2]->p_Set2(FLOAT(0.0),FLOAT(1.0));
	m_m_normals[3]->p_Set2(FLOAT(-1.0),FLOAT(0.0));
	m_m_centroid->p_SetZero();
}
int c_b2PolygonShape::p_GetVertexCount(){
	return m_m_vertexCount;
}
Array<c_b2Vec2* > c_b2PolygonShape::p_GetVertices(){
	return m_m_vertices;
}
void c_b2PolygonShape::p_Set5(c_b2Shape* t_other){
	c_b2Shape::p_Set5(t_other);
	if((dynamic_cast<c_b2PolygonShape*>(t_other))!=0){
		c_b2PolygonShape* t_other2=dynamic_cast<c_b2PolygonShape*>(t_other);
		m_m_centroid->p_SetV(t_other2->m_m_centroid);
		m_m_vertexCount=t_other2->m_m_vertexCount;
		p_Reserve(m_m_vertexCount);
		for(int t_i=0;t_i<m_m_vertexCount;t_i=t_i+1){
			m_m_vertices[t_i]->p_SetV(t_other2->m_m_vertices[t_i]);
			m_m_normals[t_i]->p_SetV(t_other2->m_m_normals[t_i]);
		}
	}
}
c_b2Shape* c_b2PolygonShape::p_Copy(){
	c_b2PolygonShape* t_s=(new c_b2PolygonShape)->m_new();
	t_s->p_Set5(this);
	return (t_s);
}
void c_b2PolygonShape::p_ComputeAABB(c_b2AABB* t_aabb,c_b2Transform* t_xf){
	c_b2Mat22* t_tMat=t_xf->m_R;
	c_b2Vec2* t_tVec=m_m_vertices[0];
	Float t_lowerX=t_xf->m_position->m_x+(t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y);
	Float t_lowerY=t_xf->m_position->m_y+(t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y);
	Float t_upperX=t_lowerX;
	Float t_upperY=t_lowerY;
	for(int t_i=1;t_i<m_m_vertexCount;t_i=t_i+1){
		t_tVec=m_m_vertices[t_i];
		Float t_vX=t_xf->m_position->m_x+(t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y);
		Float t_vY=t_xf->m_position->m_y+(t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y);
		if(t_lowerX>t_vX){
			t_lowerX=t_vX;
		}
		if(t_lowerY>t_vY){
			t_lowerY=t_vY;
		}
		if(t_upperX<t_vX){
			t_upperX=t_vX;
		}
		if(t_upperY<t_vY){
			t_upperY=t_vY;
		}
	}
	t_aabb->m_lowerBound->m_x=t_lowerX-m_m_radius;
	t_aabb->m_lowerBound->m_y=t_lowerY-m_m_radius;
	t_aabb->m_upperBound->m_x=t_upperX+m_m_radius;
	t_aabb->m_upperBound->m_y=t_upperY+m_m_radius;
}
void c_b2PolygonShape::p_ComputeMass(c_b2MassData* t_massData,Float t_density){
	if(m_m_vertexCount==2){
		t_massData->m_center->m_x=FLOAT(0.5)*(m_m_vertices[0]->m_x+m_m_vertices[1]->m_x);
		t_massData->m_center->m_y=FLOAT(0.5)*(m_m_vertices[0]->m_y+m_m_vertices[1]->m_y);
		t_massData->m_mass=FLOAT(0.0);
		t_massData->m_I=FLOAT(0.0);
		return;
	}
	Float t_centerX=FLOAT(0.0);
	Float t_centerY=FLOAT(0.0);
	Float t_area=FLOAT(0.0);
	Float t_I=FLOAT(0.0);
	Float t_p1X=FLOAT(0.0);
	Float t_p1Y=FLOAT(0.0);
	Float t_k_inv3=FLOAT(0.33333333333333331);
	for(int t_i2=0;t_i2<m_m_vertexCount;t_i2=t_i2+1){
		c_b2Vec2* t_p2=m_m_vertices[t_i2];
		c_b2Vec2* t_p3=m_m_vertices[0];
		if(t_i2+1<m_m_vertexCount){
			t_p3=m_m_vertices[t_i2+1];
		}
		Float t_e1X=t_p2->m_x-t_p1X;
		Float t_e1Y=t_p2->m_y-t_p1Y;
		Float t_e2X=t_p3->m_x-t_p1X;
		Float t_e2Y=t_p3->m_y-t_p1Y;
		Float t_D=t_e1X*t_e2Y-t_e1Y*t_e2X;
		Float t_triangleArea=FLOAT(0.5)*t_D;
		t_area+=t_triangleArea;
		t_centerX+=t_triangleArea*t_k_inv3*(t_p1X+t_p2->m_x+t_p3->m_x);
		t_centerY+=t_triangleArea*t_k_inv3*(t_p1Y+t_p2->m_y+t_p3->m_y);
		Float t_px=t_p1X;
		Float t_py=t_p1Y;
		Float t_ex1=t_e1X;
		Float t_ey1=t_e1Y;
		Float t_ex2=t_e2X;
		Float t_ey2=t_e2Y;
		Float t_intx2=t_k_inv3*(FLOAT(0.25)*(t_ex1*t_ex1+t_ex2*t_ex1+t_ex2*t_ex2)+(t_px*t_ex1+t_px*t_ex2))+FLOAT(0.5)*t_px*t_px;
		Float t_inty2=t_k_inv3*(FLOAT(0.25)*(t_ey1*t_ey1+t_ey2*t_ey1+t_ey2*t_ey2)+(t_py*t_ey1+t_py*t_ey2))+FLOAT(0.5)*t_py*t_py;
		t_I+=t_D*(t_intx2+t_inty2);
	}
	t_massData->m_mass=t_density*t_area;
	t_centerX*=FLOAT(1.0)/t_area;
	t_centerY*=FLOAT(1.0)/t_area;
	t_massData->m_center->p_Set2(t_centerX,t_centerY);
	t_massData->m_I=t_density*t_I;
}
void c_b2PolygonShape::mark(){
	c_b2Shape::mark();
	gc_mark_q(m_m_centroid);
	gc_mark_q(m_m_vertices);
	gc_mark_q(m_m_depths);
	gc_mark_q(m_m_normals);
}
c_b2CircleShape::c_b2CircleShape(){
	m_m_p=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
}
c_b2CircleShape* c_b2CircleShape::m_new(Float t_radius){
	c_b2Shape::m_new();
	m_m_type=0;
	m_m_radius=t_radius;
	return this;
}
c_b2Shape* c_b2CircleShape::p_Copy(){
	c_b2Shape* t_s=((new c_b2CircleShape)->m_new(this->m_m_radius));
	t_s->p_Set5(this);
	return t_s;
}
void c_b2CircleShape::p_Set5(c_b2Shape* t_other){
	c_b2Shape::p_Set5(t_other);
	if((dynamic_cast<c_b2CircleShape*>(t_other))!=0){
		c_b2CircleShape* t_other2=dynamic_cast<c_b2CircleShape*>(t_other);
		m_m_p->p_SetV(t_other2->m_m_p);
	}
}
void c_b2CircleShape::p_ComputeAABB(c_b2AABB* t_aabb,c_b2Transform* t_transform){
	c_b2Mat22* t_tMat=t_transform->m_R;
	Float t_pX=t_transform->m_position->m_x+(t_tMat->m_col1->m_x*m_m_p->m_x+t_tMat->m_col2->m_x*m_m_p->m_y);
	Float t_pY=t_transform->m_position->m_y+(t_tMat->m_col1->m_y*m_m_p->m_x+t_tMat->m_col2->m_y*m_m_p->m_y);
	t_aabb->m_lowerBound->p_Set2(t_pX-m_m_radius,t_pY-m_m_radius);
	t_aabb->m_upperBound->p_Set2(t_pX+m_m_radius,t_pY+m_m_radius);
}
void c_b2CircleShape::p_ComputeMass(c_b2MassData* t_massData,Float t_density){
	t_massData->m_mass=t_density*FLOAT(3.14159265)*m_m_radius*m_m_radius;
	t_massData->m_center->p_SetV(m_m_p);
	t_massData->m_I=t_massData->m_mass*(FLOAT(0.5)*m_m_radius*m_m_radius+(m_m_p->m_x*m_m_p->m_x+m_m_p->m_y*m_m_p->m_y));
}
void c_b2CircleShape::mark(){
	c_b2Shape::mark();
	gc_mark_q(m_m_p);
}
c_b2FixtureDef::c_b2FixtureDef(){
	m_shape=0;
	m_userData=0;
	m_friction=FLOAT(.0);
	m_restitution=FLOAT(.0);
	m_density=FLOAT(.0);
	m_filter=(new c_b2FilterData)->m_new();
	m_isSensor=false;
}
c_b2FixtureDef* c_b2FixtureDef::m_new(){
	m_shape=0;
	m_userData=0;
	m_friction=FLOAT(0.2);
	m_restitution=FLOAT(0.0);
	m_density=FLOAT(0.0);
	m_filter->m_categoryBits=1;
	m_filter->m_maskBits=65535;
	m_filter->m_groupIndex=0;
	m_isSensor=false;
	return this;
}
void c_b2FixtureDef::mark(){
	Object::mark();
	gc_mark_q(m_shape);
	gc_mark_q(m_userData);
	gc_mark_q(m_filter);
}
c_Polygon::c_Polygon(){
	m_vertices=Array<c_b2Vec2* >();
	m_count=0;
}
c_Polygon* c_Polygon::m_new(Array<c_b2Vec2* > t_vertices,int t_vertexCount){
	gc_assign(this->m_vertices,t_vertices);
	this->m_count=t_vertexCount;
	return this;
}
c_Polygon* c_Polygon::m_new2(){
	return this;
}
void c_Polygon::mark(){
	Object::mark();
	gc_mark_q(m_vertices);
}
c_List3::c_List3(){
	m__head=((new c_HeadNode3)->m_new());
}
c_List3* c_List3::m_new(){
	return this;
}
c_Node4* c_List3::p_AddLast3(c_Polygon* t_data){
	return (new c_Node4)->m_new(m__head,m__head->m__pred,t_data);
}
c_List3* c_List3::m_new2(Array<c_Polygon* > t_data){
	Array<c_Polygon* > t_=t_data;
	int t_2=0;
	while(t_2<t_.Length()){
		c_Polygon* t_t=t_[t_2];
		t_2=t_2+1;
		p_AddLast3(t_t);
	}
	return this;
}
c_Enumerator2* c_List3::p_ObjectEnumerator(){
	return (new c_Enumerator2)->m_new(this);
}
void c_List3::mark(){
	Object::mark();
	gc_mark_q(m__head);
}
c_Node4::c_Node4(){
	m__succ=0;
	m__pred=0;
	m__data=0;
}
c_Node4* c_Node4::m_new(c_Node4* t_succ,c_Node4* t_pred,c_Polygon* t_data){
	gc_assign(m__succ,t_succ);
	gc_assign(m__pred,t_pred);
	gc_assign(m__succ->m__pred,this);
	gc_assign(m__pred->m__succ,this);
	gc_assign(m__data,t_data);
	return this;
}
c_Node4* c_Node4::m_new2(){
	return this;
}
void c_Node4::mark(){
	Object::mark();
	gc_mark_q(m__succ);
	gc_mark_q(m__pred);
	gc_mark_q(m__data);
}
c_HeadNode3::c_HeadNode3(){
}
c_HeadNode3* c_HeadNode3::m_new(){
	c_Node4::m_new2();
	gc_assign(m__succ,(this));
	gc_assign(m__pred,(this));
	return this;
}
void c_HeadNode3::mark(){
	c_Node4::mark();
}
c_Enumerator2::c_Enumerator2(){
	m__list=0;
	m__curr=0;
}
c_Enumerator2* c_Enumerator2::m_new(c_List3* t_list){
	gc_assign(m__list,t_list);
	gc_assign(m__curr,t_list->m__head->m__succ);
	return this;
}
c_Enumerator2* c_Enumerator2::m_new2(){
	return this;
}
bool c_Enumerator2::p_HasNext(){
	while(m__curr->m__succ->m__pred!=m__curr){
		gc_assign(m__curr,m__curr->m__succ);
	}
	return m__curr!=m__list->m__head;
}
c_Polygon* c_Enumerator2::p_NextObject(){
	c_Polygon* t_data=m__curr->m__data;
	gc_assign(m__curr,m__curr->m__succ);
	return t_data;
}
void c_Enumerator2::mark(){
	Object::mark();
	gc_mark_q(m__list);
	gc_mark_q(m__curr);
}
c_b2Math::c_b2Math(){
}
void c_b2Math::m_SubtractVV(c_b2Vec2* t_a,c_b2Vec2* t_b,c_b2Vec2* t_out){
	t_out->m_x=t_a->m_x-t_b->m_x;
	t_out->m_y=t_a->m_y-t_b->m_y;
}
void c_b2Math::m_CrossVF(c_b2Vec2* t_a,Float t_s,c_b2Vec2* t_out){
	Float t_tmp=t_a->m_x;
	t_out->m_x=t_s*t_a->m_y;
	t_out->m_y=-t_s*t_tmp;
}
void c_b2Math::m_MulMV(c_b2Mat22* t_A,c_b2Vec2* t_v,c_b2Vec2* t_out){
	Float t_tmp=t_A->m_col1->m_y*t_v->m_x+t_A->m_col2->m_y*t_v->m_y;
	t_out->m_x=t_A->m_col1->m_x*t_v->m_x+t_A->m_col2->m_x*t_v->m_y;
	t_out->m_y=t_tmp;
}
void c_b2Math::m_MulX(c_b2Transform* t_T,c_b2Vec2* t_v,c_b2Vec2* t_out){
	m_MulMV(t_T->m_R,t_v,t_out);
	t_out->m_x+=t_T->m_position->m_x;
	t_out->m_y+=t_T->m_position->m_y;
}
Float c_b2Math::m_Min(Float t_a,Float t_b){
	if(t_a<t_b){
		return t_a;
	}else{
		return t_b;
	}
}
Float c_b2Math::m_Max(Float t_a,Float t_b){
	if(t_a>t_b){
		return t_a;
	}else{
		return t_b;
	}
}
Float c_b2Math::m_CrossVV(c_b2Vec2* t_a,c_b2Vec2* t_b){
	return t_a->m_x*t_b->m_y-t_a->m_y*t_b->m_x;
}
Float c_b2Math::m_Dot(c_b2Vec2* t_a,c_b2Vec2* t_b){
	return t_a->m_x*t_b->m_x+t_a->m_y*t_b->m_y;
}
void c_b2Math::m_CrossFV(Float t_s,c_b2Vec2* t_a,c_b2Vec2* t_out){
	Float t_tmp=t_a->m_x;
	t_out->m_x=-t_s*t_a->m_y;
	t_out->m_y=t_s*t_tmp;
}
void c_b2Math::m_MulTMV(c_b2Mat22* t_A,c_b2Vec2* t_v,c_b2Vec2* t_out){
	Float t_tmp=m_Dot(t_v,t_A->m_col2);
	t_out->m_x=m_Dot(t_v,t_A->m_col1);
	t_out->m_y=t_tmp;
}
Float c_b2Math::m_Clamp(Float t_a,Float t_low,Float t_high){
	if(t_a<t_low){
		return t_low;
	}else{
		if(t_a>t_high){
			return t_high;
		}else{
			return t_a;
		}
	}
}
void c_b2Math::m_AddVV(c_b2Vec2* t_a,c_b2Vec2* t_b,c_b2Vec2* t_out){
	t_out->m_x=t_a->m_x+t_b->m_x;
	t_out->m_y=t_a->m_y+t_b->m_y;
}
Float c_b2Math::m_Abs(Float t_a){
	if(t_a>FLOAT(0.0)){
		return t_a;
	}else{
		return -t_a;
	}
}
void c_b2Math::mark(){
	Object::mark();
}
c_Constants::c_Constants(){
}
void c_Constants::mark(){
	Object::mark();
}
c_b2FilterData::c_b2FilterData(){
	m_categoryBits=1;
	m_maskBits=65535;
	m_groupIndex=0;
}
c_b2FilterData* c_b2FilterData::m_new(){
	return this;
}
c_b2FilterData* c_b2FilterData::p_Copy(){
	c_b2FilterData* t_copy=(new c_b2FilterData)->m_new();
	t_copy->m_categoryBits=m_categoryBits;
	t_copy->m_maskBits=m_maskBits;
	t_copy->m_groupIndex=m_groupIndex;
	return t_copy;
}
void c_b2FilterData::mark(){
	Object::mark();
}
c_b2AABB::c_b2AABB(){
	m_lowerBound=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_upperBound=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
}
c_b2AABB* c_b2AABB::m_new(){
	return this;
}
void c_b2AABB::p_Combine(c_b2AABB* t_aabb1,c_b2AABB* t_aabb2){
	m_lowerBound->m_x=c_b2Math::m_Min(t_aabb1->m_lowerBound->m_x,t_aabb2->m_lowerBound->m_x);
	m_lowerBound->m_y=c_b2Math::m_Min(t_aabb1->m_lowerBound->m_y,t_aabb2->m_lowerBound->m_y);
	m_upperBound->m_x=c_b2Math::m_Max(t_aabb1->m_upperBound->m_x,t_aabb2->m_upperBound->m_x);
	m_upperBound->m_y=c_b2Math::m_Max(t_aabb1->m_upperBound->m_y,t_aabb2->m_upperBound->m_y);
}
bool c_b2AABB::p_TestOverlap2(c_b2AABB* t_other){
	if(t_other->m_lowerBound->m_x>m_upperBound->m_x){
		return false;
	}
	if(m_lowerBound->m_x>t_other->m_upperBound->m_x){
		return false;
	}
	if(t_other->m_lowerBound->m_y>m_upperBound->m_y){
		return false;
	}
	if(m_lowerBound->m_y>t_other->m_upperBound->m_y){
		return false;
	}
	return true;
}
void c_b2AABB::p_GetCenter(c_b2Vec2* t_out){
	t_out->m_x=(m_lowerBound->m_x+m_upperBound->m_x)*FLOAT(0.5);
	t_out->m_y=(m_lowerBound->m_y+m_upperBound->m_y)*FLOAT(0.5);
}
bool c_b2AABB::p_Contains2(c_b2AABB* t_aabb){
	bool t_result=true;
	t_result=t_result && m_lowerBound->m_x<=t_aabb->m_lowerBound->m_x;
	t_result=t_result && m_lowerBound->m_y<=t_aabb->m_lowerBound->m_y;
	t_result=t_result && t_aabb->m_upperBound->m_x<=m_upperBound->m_x;
	t_result=t_result && t_aabb->m_upperBound->m_y<=m_upperBound->m_y;
	return t_result;
}
c_b2AABB* c_b2AABB::m_StaticCombine(c_b2AABB* t_aabb1,c_b2AABB* t_aabb2){
	c_b2AABB* t_aabb=(new c_b2AABB)->m_new();
	t_aabb->p_Combine(t_aabb1,t_aabb2);
	return t_aabb;
}
void c_b2AABB::mark(){
	Object::mark();
	gc_mark_q(m_lowerBound);
	gc_mark_q(m_upperBound);
}
c_b2MassData::c_b2MassData(){
	m_mass=FLOAT(0.0);
	m_center=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_I=FLOAT(0.0);
}
c_b2MassData* c_b2MassData::m_new(){
	return this;
}
void c_b2MassData::mark(){
	Object::mark();
	gc_mark_q(m_center);
}
int bb_app__updateRate;
void bb_app_SetUpdateRate(int t_hertz){
	bb_app__updateRate=t_hertz;
	bb_app__game->SetUpdateRate(t_hertz);
}
void bb_app_ShowMouse(){
	bb_app__game->SetMouseVisible(true);
}
int bb_audio_PlayMusic(String t_path,int t_flags){
	return bb_audio_device->PlayMusic(bb_data_FixDataPath(t_path),t_flags);
}
int bb_audio_PauseMusic(){
	bb_audio_device->PauseMusic();
	return 0;
}
int bb_input_KeyDown(int t_key){
	return ((bb_input_device->p_KeyDown(t_key))?1:0);
}
Float bb_input_JoyZ(int t_index,int t_unit){
	return bb_input_device->p_JoyZ(t_index,t_unit);
}
int bb_input_JoyDown(int t_button,int t_unit){
	return ((bb_input_device->p_KeyDown(256|t_unit<<5|t_button))?1:0);
}
int bb_input_JoyHit(int t_button,int t_unit){
	return bb_input_device->p_KeyHit(256|t_unit<<5|t_button);
}
int bb_app_Millisecs(){
	return bb_app__game->Millisecs();
}
c_Enumerator3::c_Enumerator3(){
	m__list=0;
	m__curr=0;
}
c_Enumerator3* c_Enumerator3::m_new(c_List2* t_list){
	gc_assign(m__list,t_list);
	gc_assign(m__curr,t_list->m__head->m__succ);
	return this;
}
c_Enumerator3* c_Enumerator3::m_new2(){
	return this;
}
bool c_Enumerator3::p_HasNext(){
	while(m__curr->m__succ->m__pred!=m__curr){
		gc_assign(m__curr,m__curr->m__succ);
	}
	return m__curr!=m__list->m__head;
}
c_Barrier* c_Enumerator3::p_NextObject(){
	c_Barrier* t_data=m__curr->m__data;
	gc_assign(m__curr,m__curr->m__succ);
	return t_data;
}
void c_Enumerator3::mark(){
	Object::mark();
	gc_mark_q(m__list);
	gc_mark_q(m__curr);
}
c_List3* bb_Rebound_CreateCross1(){
	Float t_scale=FLOAT(64.0);
	c_List3* t_pList=(new c_List3)->m_new();
	Array<c_b2Vec2* > t_vertices=Array<c_b2Vec2* >();
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(115.500)/t_scale,FLOAT(14.500)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-14.500)/t_scale,FLOAT(14.500)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-115.500)/t_scale,FLOAT(-14.500)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(115.500)/t_scale,FLOAT(-14.500)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-14.500)/t_scale,FLOAT(115.500)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-14.500)/t_scale,FLOAT(14.500)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(14.500)/t_scale,FLOAT(14.500)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(14.500)/t_scale,FLOAT(115.500)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	t_vertices=Array<c_b2Vec2* >(3);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-115.500)/t_scale,FLOAT(-14.500)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-14.500)/t_scale,FLOAT(14.500)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(-115.500)/t_scale,FLOAT(14.500)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,3));
	t_vertices=Array<c_b2Vec2* >(4);
	gc_assign(t_vertices[0],(new c_b2Vec2)->m_new(FLOAT(-14.500)/t_scale,FLOAT(-14.500)/t_scale));
	gc_assign(t_vertices[1],(new c_b2Vec2)->m_new(FLOAT(-14.500)/t_scale,FLOAT(-115.500)/t_scale));
	gc_assign(t_vertices[2],(new c_b2Vec2)->m_new(FLOAT(14.500)/t_scale,FLOAT(-115.500)/t_scale));
	gc_assign(t_vertices[3],(new c_b2Vec2)->m_new(FLOAT(14.500)/t_scale,FLOAT(-14.500)/t_scale));
	t_pList->p_AddLast3((new c_Polygon)->m_new(t_vertices,4));
	return t_pList;
}
c_StringObject::c_StringObject(){
	m_value=String();
}
c_StringObject* c_StringObject::m_new(int t_value){
	this->m_value=String(t_value);
	return this;
}
c_StringObject* c_StringObject::m_new2(Float t_value){
	this->m_value=String(t_value);
	return this;
}
c_StringObject* c_StringObject::m_new3(String t_value){
	this->m_value=t_value;
	return this;
}
c_StringObject* c_StringObject::m_new4(){
	return this;
}
void c_StringObject::mark(){
	Object::mark();
}
c_SensorContactListener::c_SensorContactListener(){
	m_sensor=0;
	m_player=0;
}
c_SensorContactListener* c_SensorContactListener::m_new(c_b2Body* t_sensor,c_b2Body* t_player){
	c_b2ContactListener::m_new();
	gc_assign(this->m_sensor,t_sensor);
	gc_assign(this->m_player,t_player);
	return this;
}
c_SensorContactListener* c_SensorContactListener::m_new2(){
	c_b2ContactListener::m_new();
	return this;
}
void c_SensorContactListener::p_BeginContact(c_b2Contact* t_contact){
	if(t_contact->p_GetFixtureA()->p_GetBody()==m_sensor || t_contact->p_GetFixtureB()->p_GetBody()==m_sensor){
		if(t_contact->p_GetFixtureA()->p_GetBody()==m_player || t_contact->p_GetFixtureB()->p_GetBody()==m_player){
			c_Game::m_sensorColliding=true;
		}
	}
}
void c_SensorContactListener::p_EndContact(c_b2Contact* t_contact){
	if(t_contact->p_GetFixtureA()->p_GetBody()==m_sensor || t_contact->p_GetFixtureB()->p_GetBody()==m_sensor){
		if(t_contact->p_GetFixtureA()->p_GetBody()==m_player || t_contact->p_GetFixtureB()->p_GetBody()==m_player){
			c_Game::m_sensorColliding=false;
		}
	}
}
void c_SensorContactListener::mark(){
	c_b2ContactListener::mark();
	gc_mark_q(m_sensor);
	gc_mark_q(m_player);
}
int bb_input_TouchHit(int t_index){
	return bb_input_device->p_KeyHit(384+t_index);
}
int bb_input_KeyHit(int t_key){
	return bb_input_device->p_KeyHit(t_key);
}
int bb_Rebound_aStars(Float t_gold,int t_silver,Float t_time){
	int t_stars=1;
	if(t_time<=t_gold){
		t_stars=3;
	}else{
		if(t_time>t_gold && t_time<=Float(t_silver)){
			t_stars=2;
		}else{
			t_stars=1;
		}
	}
	return t_stars;
}
int bb_Rebound_AssignStars(int t_stage,int t_level,Float t_time){
	int t_stars=1;
	t_time=t_time/FLOAT(1000.0);
	if(t_stage==1){
		if(t_level==1){
			t_stars=bb_Rebound_aStars(FLOAT(5.0),7,t_time);
		}else{
			if(t_level==2){
				t_stars=bb_Rebound_aStars(FLOAT(7.0),9,t_time);
			}else{
				if(t_level==3){
					t_stars=bb_Rebound_aStars(FLOAT(6.0),8,t_time);
				}else{
					if(t_level==4){
						t_stars=bb_Rebound_aStars(FLOAT(6.0),9,t_time);
					}else{
						if(t_level==5){
							t_stars=bb_Rebound_aStars(FLOAT(5.0),8,t_time);
						}else{
							if(t_level==6){
								t_stars=bb_Rebound_aStars(FLOAT(4.0),8,t_time);
							}else{
								if(t_level==7){
									t_stars=bb_Rebound_aStars(FLOAT(8.0),11,t_time);
								}else{
									if(t_level==8){
										t_stars=bb_Rebound_aStars(FLOAT(7.0),12,t_time);
									}
								}
							}
						}
					}
				}
			}
		}
	}else{
		if(t_stage==2){
			if(t_level==1){
				t_stars=bb_Rebound_aStars(FLOAT(10.0),14,t_time);
			}else{
				if(t_level==2){
					t_stars=bb_Rebound_aStars(FLOAT(9.0),12,t_time);
				}else{
					if(t_level==3){
						t_stars=bb_Rebound_aStars(FLOAT(7.0),10,t_time);
					}else{
						if(t_level==4){
							t_stars=bb_Rebound_aStars(FLOAT(3.0),6,t_time);
						}else{
							if(t_level==5){
								t_stars=bb_Rebound_aStars(FLOAT(5.5),9,t_time);
							}else{
								if(t_level==6){
									t_stars=bb_Rebound_aStars(FLOAT(6.5),9,t_time);
								}else{
									if(t_level==7){
										t_stars=bb_Rebound_aStars(FLOAT(3.5),6,t_time);
									}else{
										if(t_level==8){
											t_stars=bb_Rebound_aStars(FLOAT(9.5),15,t_time);
										}
									}
								}
							}
						}
					}
				}
			}
		}else{
			if(t_stage==3){
				if(t_level==1){
					t_stars=bb_Rebound_aStars(FLOAT(6.5),7,t_time);
				}else{
					if(t_level==2){
						t_stars=bb_Rebound_aStars(FLOAT(25.0),40,t_time);
					}else{
						if(t_level==3){
							t_stars=bb_Rebound_aStars(FLOAT(4.5),6,t_time);
						}else{
							if(t_level==4){
								t_stars=bb_Rebound_aStars(FLOAT(9.5),12,t_time);
							}else{
								if(t_level==5){
									t_stars=bb_Rebound_aStars(FLOAT(4.5),8,t_time);
								}else{
									if(t_level==6){
										t_stars=bb_Rebound_aStars(FLOAT(7.1),10,t_time);
									}else{
										if(t_level==7){
											t_stars=bb_Rebound_aStars(FLOAT(8.0),13,t_time);
										}else{
											if(t_level==8){
												t_stars=bb_Rebound_aStars(FLOAT(7.0),12,t_time);
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return t_stars;
}
c_b2TimeStep::c_b2TimeStep(){
	m_dt=FLOAT(.0);
	m_velocityIterations=0;
	m_positionIterations=0;
	m_inv_dt=FLOAT(.0);
	m_dtRatio=FLOAT(.0);
	m_warmStarting=false;
}
c_b2TimeStep* c_b2TimeStep::m_new(){
	return this;
}
void c_b2TimeStep::p_Set9(c_b2TimeStep* t_timeStep){
	m_dt=t_timeStep->m_dt;
	m_inv_dt=t_timeStep->m_inv_dt;
	m_positionIterations=t_timeStep->m_positionIterations;
	m_velocityIterations=t_timeStep->m_velocityIterations;
	m_warmStarting=t_timeStep->m_warmStarting;
}
void c_b2TimeStep::mark(){
	Object::mark();
}
c_b2DistanceInput::c_b2DistanceInput(){
	m_proxyA=0;
	m_proxyB=0;
	m_transformA=0;
	m_transformB=0;
	m_useRadii=false;
}
c_b2DistanceInput* c_b2DistanceInput::m_new(){
	return this;
}
void c_b2DistanceInput::mark(){
	Object::mark();
	gc_mark_q(m_proxyA);
	gc_mark_q(m_proxyB);
	gc_mark_q(m_transformA);
	gc_mark_q(m_transformB);
}
c_b2DistanceProxy::c_b2DistanceProxy(){
	m_m_vertices=Array<c_b2Vec2* >();
	m_m_count=0;
	m_m_radius=FLOAT(.0);
}
c_b2DistanceProxy* c_b2DistanceProxy::m_new(){
	return this;
}
void c_b2DistanceProxy::p_Set5(c_b2Shape* t_shape){
	int t_1=t_shape->p_GetType();
	if(t_1==0){
		c_b2CircleShape* t_circle=dynamic_cast<c_b2CircleShape*>(t_shape);
		gc_assign(m_m_vertices,Array<c_b2Vec2* >(1));
		gc_assign(m_m_vertices[0],t_circle->m_m_p);
		m_m_count=1;
		m_m_radius=t_circle->m_m_radius;
	}else{
		if(t_1==1){
			c_b2PolygonShape* t_polygon=dynamic_cast<c_b2PolygonShape*>(t_shape);
			gc_assign(m_m_vertices,t_polygon->m_m_vertices);
			m_m_count=t_polygon->m_m_vertexCount;
			m_m_radius=t_polygon->m_m_radius;
		}else{
			c_b2Settings::m_B2Assert(false);
		}
	}
}
c_b2Vec2* c_b2DistanceProxy::p_GetVertex(int t_index){
	c_b2Settings::m_B2Assert(0<=t_index && t_index<m_m_count);
	return m_m_vertices[t_index];
}
Float c_b2DistanceProxy::p_GetSupport(c_b2Vec2* t_d){
	int t_bestIndex=0;
	Float t_bestValue=m_m_vertices[0]->m_x*t_d->m_x+m_m_vertices[0]->m_y*t_d->m_y;
	for(int t_i=1;t_i<m_m_count;t_i=t_i+1){
		Float t_value=m_m_vertices[t_i]->m_x*t_d->m_x+m_m_vertices[t_i]->m_y*t_d->m_y;
		if(t_value>t_bestValue){
			t_bestIndex=t_i;
			t_bestValue=t_value;
		}
	}
	return Float(t_bestIndex);
}
c_b2Vec2* c_b2DistanceProxy::p_GetSupportVertex(c_b2Vec2* t_d){
	int t_bestIndex=0;
	Float t_bestValue=m_m_vertices[0]->m_x*t_d->m_x+m_m_vertices[0]->m_y*t_d->m_y;
	for(int t_i=1;t_i<m_m_count;t_i=t_i+1){
		Float t_value=m_m_vertices[t_i]->m_x*t_d->m_x+m_m_vertices[t_i]->m_y*t_d->m_y;
		if(t_value>t_bestValue){
			t_bestIndex=t_i;
			t_bestValue=t_value;
		}
	}
	return m_m_vertices[t_bestIndex];
}
void c_b2DistanceProxy::mark(){
	Object::mark();
	gc_mark_q(m_m_vertices);
}
c_b2SimplexCache::c_b2SimplexCache(){
	m_count=0;
	m_indexA=Array<int >(3);
	m_indexB=Array<int >(3);
	m_metric=FLOAT(.0);
}
c_b2SimplexCache* c_b2SimplexCache::m_new(){
	return this;
}
void c_b2SimplexCache::mark(){
	Object::mark();
	gc_mark_q(m_indexA);
	gc_mark_q(m_indexB);
}
c_b2DistanceOutput::c_b2DistanceOutput(){
	m_pointA=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_pointB=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_distance=FLOAT(.0);
	m_iterations=0;
}
c_b2DistanceOutput* c_b2DistanceOutput::m_new(){
	return this;
}
void c_b2DistanceOutput::mark(){
	Object::mark();
	gc_mark_q(m_pointA);
	gc_mark_q(m_pointB);
}
c_b2Distance::c_b2Distance(){
}
int c_b2Distance::m_b2_gjkCalls;
c_b2Simplex* c_b2Distance::m_s_simplex;
Array<int > c_b2Distance::m_s_saveA;
Array<int > c_b2Distance::m_s_saveB;
c_b2Vec2* c_b2Distance::m_tmpVec1;
c_b2Vec2* c_b2Distance::m_tmpVec2;
int c_b2Distance::m_b2_gjkIters;
int c_b2Distance::m_b2_gjkMaxIters;
void c_b2Distance::m_Distance(c_b2DistanceOutput* t_output,c_b2SimplexCache* t_cache,c_b2DistanceInput* t_input){
	m_b2_gjkCalls+=1;
	c_b2DistanceProxy* t_proxyA=t_input->m_proxyA;
	c_b2DistanceProxy* t_proxyB=t_input->m_proxyB;
	c_b2Transform* t_transformA=t_input->m_transformA;
	c_b2Transform* t_transformB=t_input->m_transformB;
	c_b2Simplex* t_simplex=m_s_simplex;
	t_simplex->p_ReadCache(t_cache,t_proxyA,t_transformA,t_proxyB,t_transformB);
	Array<c_b2SimplexVertex* > t_vertices=t_simplex->m_m_vertices;
	Array<int > t_saveA=m_s_saveA;
	Array<int > t_saveB=m_s_saveB;
	int t_saveCount=0;
	t_simplex->p_GetClosestPoint(m_tmpVec1);
	Float t_distanceSqr1=m_tmpVec1->p_LengthSquared();
	Float t_distanceSqr2=t_distanceSqr1;
	int t_i=0;
	int t_iter=0;
	while(t_iter<20){
		t_saveCount=t_simplex->m_m_count;
		for(int t_i2=0;t_i2<t_saveCount;t_i2=t_i2+1){
			t_saveA[t_i2]=t_vertices[t_i2]->m_indexA;
			t_saveB[t_i2]=t_vertices[t_i2]->m_indexB;
		}
		int t_1=t_simplex->m_m_count;
		if(t_1==1){
		}else{
			if(t_1==2){
				t_simplex->p_Solve2();
			}else{
				if(t_1==3){
					t_simplex->p_Solve3();
				}else{
					c_b2Settings::m_B2Assert(false);
				}
			}
		}
		if(t_simplex->m_m_count==3){
			break;
		}
		t_simplex->p_GetClosestPoint(m_tmpVec1);
		t_distanceSqr2=m_tmpVec1->p_LengthSquared();
		if(t_distanceSqr2>t_distanceSqr1){
		}
		t_distanceSqr1=t_distanceSqr2;
		t_simplex->p_GetSearchDirection(m_tmpVec1);
		c_b2Vec2* t_d=m_tmpVec1;
		if(t_d->p_LengthSquared()<FLOAT(1.0000000000000001e-030)){
			break;
		}
		c_b2SimplexVertex* t_vertex=t_vertices[t_simplex->m_m_count];
		t_d->p_GetNegative(m_tmpVec2);
		c_b2Math::m_MulTMV(t_transformA->m_R,m_tmpVec2,m_tmpVec2);
		t_vertex->m_indexA=int(t_proxyA->p_GetSupport(m_tmpVec2));
		c_b2Math::m_MulX(t_transformA,t_proxyA->p_GetVertex(t_vertex->m_indexA),t_vertex->m_wA);
		c_b2Math::m_MulTMV(t_transformB->m_R,t_d,m_tmpVec2);
		t_vertex->m_indexB=int(t_proxyB->p_GetSupport(m_tmpVec2));
		c_b2Math::m_MulX(t_transformB,t_proxyB->p_GetVertex(t_vertex->m_indexB),t_vertex->m_wB);
		c_b2Math::m_SubtractVV(t_vertex->m_wB,t_vertex->m_wA,t_vertex->m_w);
		t_iter+=1;
		m_b2_gjkIters+=1;
		bool t_duplicate=false;
		for(int t_i3=0;t_i3<t_saveCount;t_i3=t_i3+1){
			if(t_vertex->m_indexA==t_saveA[t_i3] && t_vertex->m_indexB==t_saveB[t_i3]){
				t_duplicate=true;
				break;
			}
		}
		if(t_duplicate){
			break;
		}
		t_simplex->m_m_count+=1;
	}
	m_b2_gjkMaxIters=int(c_b2Math::m_Max(Float(m_b2_gjkMaxIters),Float(t_iter)));
	t_simplex->p_GetWitnessPoints(t_output->m_pointA,t_output->m_pointB);
	c_b2Math::m_SubtractVV(t_output->m_pointA,t_output->m_pointB,m_tmpVec2);
	t_output->m_distance=m_tmpVec2->p_Length();
	t_output->m_iterations=t_iter;
	t_simplex->p_WriteCache(t_cache);
	if(t_input->m_useRadii){
		Float t_rA=t_proxyA->m_m_radius;
		Float t_rB=t_proxyB->m_m_radius;
		if(t_output->m_distance>t_rA+t_rB && t_output->m_distance>FLOAT(1e-15)){
			t_output->m_distance-=t_rA+t_rB;
			c_b2Vec2* t_normal=m_tmpVec2;
			c_b2Math::m_SubtractVV(t_output->m_pointB,t_output->m_pointA,t_normal);
			t_normal->p_Normalize();
			t_output->m_pointA->m_x+=t_rA*t_normal->m_x;
			t_output->m_pointA->m_y+=t_rA*t_normal->m_y;
			t_output->m_pointB->m_x-=t_rB*t_normal->m_x;
			t_output->m_pointB->m_y-=t_rB*t_normal->m_y;
		}else{
			m_tmpVec2->m_x=FLOAT(0.5)*(t_output->m_pointA->m_x+t_output->m_pointB->m_x);
			m_tmpVec2->m_y=FLOAT(0.5)*(t_output->m_pointA->m_y+t_output->m_pointB->m_y);
			t_output->m_pointA->m_x=m_tmpVec2->m_x;
			t_output->m_pointB->m_x=t_output->m_pointA->m_x;
			t_output->m_pointA->m_y=m_tmpVec2->m_y;
			t_output->m_pointB->m_y=t_output->m_pointA->m_y;
			t_output->m_distance=FLOAT(0.0);
		}
	}
}
void c_b2Distance::mark(){
	Object::mark();
}
c_b2Simplex::c_b2Simplex(){
	m_m_v1=(new c_b2SimplexVertex)->m_new();
	m_m_vertices=Array<c_b2SimplexVertex* >(3);
	m_m_v2=(new c_b2SimplexVertex)->m_new();
	m_m_v3=(new c_b2SimplexVertex)->m_new();
	m_m_count=0;
}
c_b2Simplex* c_b2Simplex::m_new(){
	gc_assign(m_m_vertices[0],m_m_v1);
	gc_assign(m_m_vertices[1],m_m_v2);
	gc_assign(m_m_vertices[2],m_m_v3);
	return this;
}
c_b2Vec2* c_b2Simplex::m_tmpVec1;
c_b2Vec2* c_b2Simplex::m_tmpVec2;
Float c_b2Simplex::p_GetMetric(){
	int t_4=m_m_count;
	if(t_4==0){
		c_b2Settings::m_B2Assert(false);
		return FLOAT(0.0);
	}else{
		if(t_4==1){
			return FLOAT(0.0);
		}else{
			if(t_4==2){
				c_b2Math::m_SubtractVV(m_m_v1->m_w,m_m_v2->m_w,m_tmpVec1);
				return m_tmpVec1->p_Length();
			}else{
				if(t_4==3){
					c_b2Math::m_SubtractVV(m_m_v2->m_w,m_m_v1->m_w,m_tmpVec1);
					c_b2Math::m_SubtractVV(m_m_v3->m_w,m_m_v1->m_w,m_tmpVec2);
					return c_b2Math::m_CrossVV(m_tmpVec1,m_tmpVec2);
				}else{
					c_b2Settings::m_B2Assert(false);
					return FLOAT(0.0);
				}
			}
		}
	}
}
void c_b2Simplex::p_ReadCache(c_b2SimplexCache* t_cache,c_b2DistanceProxy* t_proxyA,c_b2Transform* t_transformA,c_b2DistanceProxy* t_proxyB,c_b2Transform* t_transformB){
	c_b2Vec2* t_wALocal=0;
	c_b2Vec2* t_wBLocal=0;
	c_b2SimplexVertex* t_v=0;
	m_m_count=t_cache->m_count;
	Array<c_b2SimplexVertex* > t_vertices=m_m_vertices;
	for(int t_i=0;t_i<m_m_count;t_i=t_i+1){
		t_v=t_vertices[t_i];
		t_v->m_indexA=t_cache->m_indexA[t_i];
		t_v->m_indexB=t_cache->m_indexB[t_i];
		t_wALocal=t_proxyA->p_GetVertex(t_v->m_indexA);
		t_wBLocal=t_proxyB->p_GetVertex(t_v->m_indexB);
		c_b2Math::m_MulX(t_transformA,t_wALocal,t_v->m_wA);
		c_b2Math::m_MulX(t_transformB,t_wBLocal,t_v->m_wB);
		c_b2Math::m_SubtractVV(t_v->m_wB,t_v->m_wA,t_v->m_w);
		t_v->m_a=FLOAT(0.0);
	}
	if(m_m_count>1){
		Float t_metric1=t_cache->m_metric;
		Float t_metric2=p_GetMetric();
		if(t_metric2<FLOAT(0.5)*t_metric1 || FLOAT(2.0)*t_metric1<t_metric2 || t_metric2<FLOAT(1e-15)){
			m_m_count=0;
		}
	}
	if(m_m_count==0){
		t_v=t_vertices[0];
		t_v->m_indexA=0;
		t_v->m_indexB=0;
		t_wALocal=t_proxyA->p_GetVertex(0);
		t_wBLocal=t_proxyB->p_GetVertex(0);
		c_b2Math::m_MulX(t_transformA,t_wALocal,t_v->m_wA);
		c_b2Math::m_MulX(t_transformB,t_wBLocal,t_v->m_wB);
		c_b2Math::m_SubtractVV(t_v->m_wB,t_v->m_wA,t_v->m_w);
		m_m_count=1;
	}
}
void c_b2Simplex::p_GetClosestPoint(c_b2Vec2* t_out){
	int t_2=m_m_count;
	if(t_2==0){
		c_b2Settings::m_B2Assert(false);
		t_out->m_x=FLOAT(0.0);
		t_out->m_y=FLOAT(0.0);
	}else{
		if(t_2==1){
			t_out->m_x=m_m_v1->m_w->m_x;
			t_out->m_y=m_m_v1->m_w->m_y;
		}else{
			if(t_2==2){
				t_out->m_x=m_m_v1->m_a*m_m_v1->m_w->m_x+m_m_v2->m_a*m_m_v2->m_w->m_x;
				t_out->m_y=m_m_v1->m_a*m_m_v1->m_w->m_y+m_m_v2->m_a*m_m_v2->m_w->m_y;
			}else{
				c_b2Settings::m_B2Assert(false);
				t_out->m_x=FLOAT(0.0);
				t_out->m_y=FLOAT(0.0);
			}
		}
	}
}
void c_b2Simplex::p_Solve2(){
	c_b2Vec2* t_w1=m_m_v1->m_w;
	c_b2Vec2* t_w2=m_m_v2->m_w;
	c_b2Math::m_SubtractVV(t_w2,t_w1,m_tmpVec1);
	c_b2Vec2* t_e12=m_tmpVec1;
	Float t_d12_2=-(t_w1->m_x*t_e12->m_x+t_w1->m_y*t_e12->m_y);
	if(t_d12_2<=FLOAT(0.0)){
		m_m_v1->m_a=FLOAT(1.0);
		m_m_count=1;
		return;
	}
	Float t_d12_1=t_w2->m_x*t_e12->m_x+t_w2->m_y*t_e12->m_y;
	if(t_d12_1<=FLOAT(0.0)){
		m_m_v2->m_a=FLOAT(1.0);
		m_m_count=1;
		m_m_v1->p_Set10(m_m_v2);
		return;
	}
	Float t_inv_d12=FLOAT(1.0)/(t_d12_1+t_d12_2);
	m_m_v1->m_a=t_d12_1*t_inv_d12;
	m_m_v2->m_a=t_d12_2*t_inv_d12;
	m_m_count=2;
}
c_b2Vec2* c_b2Simplex::m_tmpVec3;
void c_b2Simplex::p_Solve3(){
	c_b2Vec2* t_w1=m_m_v1->m_w;
	c_b2Vec2* t_w2=m_m_v2->m_w;
	c_b2Vec2* t_w3=m_m_v3->m_w;
	c_b2Math::m_SubtractVV(t_w2,t_w1,m_tmpVec1);
	c_b2Vec2* t_e12=m_tmpVec1;
	Float t_w1e12=c_b2Math::m_Dot(t_w1,t_e12);
	Float t_w2e12=c_b2Math::m_Dot(t_w2,t_e12);
	Float t_d12_1=t_w2e12;
	Float t_d12_2=-t_w1e12;
	c_b2Math::m_SubtractVV(t_w3,t_w1,m_tmpVec2);
	c_b2Vec2* t_e13=m_tmpVec2;
	Float t_w1e13=c_b2Math::m_Dot(t_w1,t_e13);
	Float t_w3e13=c_b2Math::m_Dot(t_w3,t_e13);
	Float t_d13_1=t_w3e13;
	Float t_d13_2=-t_w1e13;
	c_b2Math::m_SubtractVV(t_w3,t_w2,m_tmpVec3);
	c_b2Vec2* t_e23=m_tmpVec3;
	Float t_w2e23=c_b2Math::m_Dot(t_w2,t_e23);
	Float t_w3e23=c_b2Math::m_Dot(t_w3,t_e23);
	Float t_d23_1=t_w3e23;
	Float t_d23_2=-t_w2e23;
	Float t_n123=c_b2Math::m_CrossVV(t_e12,t_e13);
	Float t_d123_1=t_n123*c_b2Math::m_CrossVV(t_w2,t_w3);
	Float t_d123_2=t_n123*c_b2Math::m_CrossVV(t_w3,t_w1);
	Float t_d123_3=t_n123*c_b2Math::m_CrossVV(t_w1,t_w2);
	if(t_d12_2<=FLOAT(0.0) && t_d13_2<=FLOAT(0.0)){
		m_m_v1->m_a=FLOAT(1.0);
		m_m_count=1;
		return;
	}
	if(t_d12_1>FLOAT(0.0) && t_d12_2>FLOAT(0.0) && t_d123_3<=FLOAT(0.0)){
		Float t_inv_d12=FLOAT(1.0)/(t_d12_1+t_d12_2);
		m_m_v1->m_a=t_d12_1*t_inv_d12;
		m_m_v2->m_a=t_d12_2*t_inv_d12;
		m_m_count=2;
		return;
	}
	if(t_d13_1>FLOAT(0.0) && t_d13_2>FLOAT(0.0) && t_d123_2<=FLOAT(0.0)){
		Float t_inv_d13=FLOAT(1.0)/(t_d13_1+t_d13_2);
		m_m_v1->m_a=t_d13_1*t_inv_d13;
		m_m_v3->m_a=t_d13_2*t_inv_d13;
		m_m_count=2;
		m_m_v2->p_Set10(m_m_v3);
		return;
	}
	if(t_d12_1<=FLOAT(0.0) && t_d23_2<=FLOAT(0.0)){
		m_m_v2->m_a=FLOAT(1.0);
		m_m_count=1;
		m_m_v1->p_Set10(m_m_v2);
		return;
	}
	if(t_d13_1<=FLOAT(0.0) && t_d23_1<=FLOAT(0.0)){
		m_m_v3->m_a=FLOAT(1.0);
		m_m_count=1;
		m_m_v1->p_Set10(m_m_v3);
		return;
	}
	if(t_d23_1>FLOAT(0.0) && t_d23_2>FLOAT(0.0) && t_d123_1<=FLOAT(0.0)){
		Float t_inv_d23=FLOAT(1.0)/(t_d23_1+t_d23_2);
		m_m_v2->m_a=t_d23_1*t_inv_d23;
		m_m_v3->m_a=t_d23_2*t_inv_d23;
		m_m_count=2;
		m_m_v1->p_Set10(m_m_v3);
		return;
	}
	Float t_inv_d123=FLOAT(1.0)/(t_d123_1+t_d123_2+t_d123_3);
	m_m_v1->m_a=t_d123_1*t_inv_d123;
	m_m_v2->m_a=t_d123_2*t_inv_d123;
	m_m_v3->m_a=t_d123_3*t_inv_d123;
	m_m_count=3;
}
void c_b2Simplex::p_GetSearchDirection(c_b2Vec2* t_out){
	int t_1=m_m_count;
	if(t_1==1){
		m_m_v1->m_w->p_GetNegative(t_out);
	}else{
		if(t_1==2){
			c_b2Math::m_SubtractVV(m_m_v2->m_w,m_m_v1->m_w,t_out);
			m_m_v1->m_w->p_GetNegative(m_tmpVec1);
			Float t_sgn=c_b2Math::m_CrossVV(t_out,m_tmpVec1);
			if(t_sgn>FLOAT(0.0)){
				c_b2Math::m_CrossFV(FLOAT(1.0),t_out,t_out);
			}else{
				c_b2Math::m_CrossVF(t_out,FLOAT(1.0),t_out);
			}
		}else{
			c_b2Settings::m_B2Assert(false);
			t_out->p_Set2(FLOAT(0.0),FLOAT(0.0));
		}
	}
}
void c_b2Simplex::p_GetWitnessPoints(c_b2Vec2* t_pA,c_b2Vec2* t_pB){
	int t_3=m_m_count;
	if(t_3==0){
		c_b2Settings::m_B2Assert(false);
	}else{
		if(t_3==1){
			t_pA->p_SetV(m_m_v1->m_wA);
			t_pB->p_SetV(m_m_v1->m_wB);
		}else{
			if(t_3==2){
				t_pA->m_x=m_m_v1->m_a*m_m_v1->m_wA->m_x+m_m_v2->m_a*m_m_v2->m_wA->m_x;
				t_pA->m_y=m_m_v1->m_a*m_m_v1->m_wA->m_y+m_m_v2->m_a*m_m_v2->m_wA->m_y;
				t_pB->m_x=m_m_v1->m_a*m_m_v1->m_wB->m_x+m_m_v2->m_a*m_m_v2->m_wB->m_x;
				t_pB->m_y=m_m_v1->m_a*m_m_v1->m_wB->m_y+m_m_v2->m_a*m_m_v2->m_wB->m_y;
			}else{
				if(t_3==3){
					t_pA->m_x=m_m_v1->m_a*m_m_v1->m_wA->m_x+m_m_v2->m_a*m_m_v2->m_wA->m_x+m_m_v3->m_a*m_m_v3->m_wA->m_x;
					t_pB->m_x=t_pA->m_x;
					t_pA->m_y=m_m_v1->m_a*m_m_v1->m_wA->m_y+m_m_v2->m_a*m_m_v2->m_wA->m_y+m_m_v3->m_a*m_m_v3->m_wA->m_y;
					t_pB->m_y=t_pA->m_y;
				}else{
					c_b2Settings::m_B2Assert(false);
				}
			}
		}
	}
}
void c_b2Simplex::p_WriteCache(c_b2SimplexCache* t_cache){
	t_cache->m_metric=p_GetMetric();
	t_cache->m_count=m_m_count;
	Array<c_b2SimplexVertex* > t_vertices=m_m_vertices;
	for(int t_i=0;t_i<m_m_count;t_i=t_i+1){
		t_cache->m_indexA[t_i]=t_vertices[t_i]->m_indexA;
		t_cache->m_indexB[t_i]=t_vertices[t_i]->m_indexB;
	}
}
void c_b2Simplex::mark(){
	Object::mark();
	gc_mark_q(m_m_v1);
	gc_mark_q(m_m_vertices);
	gc_mark_q(m_m_v2);
	gc_mark_q(m_m_v3);
}
c_b2SimplexVertex::c_b2SimplexVertex(){
	m_indexA=0;
	m_indexB=0;
	m_wA=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_wB=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_w=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_a=FLOAT(.0);
}
c_b2SimplexVertex* c_b2SimplexVertex::m_new(){
	return this;
}
void c_b2SimplexVertex::p_Set10(c_b2SimplexVertex* t_other){
	m_wA->p_SetV(t_other->m_wA);
	m_wB->p_SetV(t_other->m_wB);
	m_w->p_SetV(t_other->m_w);
	m_a=t_other->m_a;
	m_indexA=t_other->m_indexA;
	m_indexB=t_other->m_indexB;
}
void c_b2SimplexVertex::mark(){
	Object::mark();
	gc_mark_q(m_wA);
	gc_mark_q(m_wB);
	gc_mark_q(m_w);
}
c_b2Island::c_b2Island(){
	m_m_bodyCapacity=0;
	m_m_bodies=Array<c_b2Body* >();
	m_m_contactCapacity=0;
	m_m_contacts=Array<c_b2Contact* >();
	m_m_jointCapacity=0;
	m_m_joints=Array<c_b2Joint* >();
	m_m_bodyCount=0;
	m_m_contactCount=0;
	m_m_jointCount=0;
	m_m_allocator=0;
	m_m_listener=0;
	m_m_contactSolver=0;
}
c_b2Island* c_b2Island::m_new(){
	return this;
}
void c_b2Island::p_Initialize(int t_bodyCapacity,int t_contactCapacity,int t_jointCapacity,Object* t_allocator,c_b2ContactListenerInterface* t_listener,c_b2ContactSolver* t_contactSolver){
	int t_i=0;
	m_m_bodyCapacity=t_bodyCapacity;
	if(m_m_bodies.Length()<m_m_bodyCapacity){
		gc_assign(m_m_bodies,m_m_bodies.Resize(m_m_bodyCapacity));
	}
	m_m_contactCapacity=t_contactCapacity;
	if(m_m_contacts.Length()<m_m_contactCapacity){
		gc_assign(m_m_contacts,m_m_contacts.Resize(m_m_contactCapacity));
	}
	m_m_jointCapacity=t_jointCapacity;
	if(m_m_joints.Length()<m_m_jointCapacity){
		gc_assign(m_m_joints,m_m_joints.Resize(m_m_jointCapacity));
	}
	m_m_bodyCount=0;
	m_m_contactCount=0;
	m_m_jointCount=0;
	gc_assign(m_m_allocator,t_allocator);
	gc_assign(m_m_listener,t_listener);
	gc_assign(m_m_contactSolver,t_contactSolver);
	for(int t_i2=m_m_bodies.Length();t_i2<t_bodyCapacity;t_i2=t_i2+1){
		m_m_bodies[t_i2]=0;
	}
	for(int t_i3=m_m_contacts.Length();t_i3<t_contactCapacity;t_i3=t_i3+1){
		m_m_contacts[t_i3]=0;
	}
	for(int t_i4=m_m_joints.Length();t_i4<t_jointCapacity;t_i4=t_i4+1){
		m_m_joints[t_i4]=0;
	}
}
void c_b2Island::p_Clear(){
	m_m_bodyCount=0;
	m_m_contactCount=0;
	m_m_jointCount=0;
}
void c_b2Island::p_AddBody(c_b2Body* t_body){
	t_body->m_m_islandIndex=m_m_bodyCount;
	gc_assign(m_m_bodies[m_m_bodyCount],t_body);
	m_m_bodyCount+=1;
}
void c_b2Island::p_AddContact(c_b2Contact* t_contact){
	gc_assign(m_m_contacts[m_m_contactCount],t_contact);
	m_m_contactCount+=1;
}
void c_b2Island::p_AddJoint(c_b2Joint* t_joint){
	gc_assign(m_m_joints[m_m_jointCount],t_joint);
	m_m_jointCount+=1;
}
c_b2ContactImpulse* c_b2Island::m_s_impulse;
void c_b2Island::p_Report(Array<c_b2ContactConstraint* > t_constraints){
	if(m_m_listener==0){
		return;
	}
	for(int t_i=0;t_i<m_m_contactCount;t_i=t_i+1){
		c_b2Contact* t_c=m_m_contacts[t_i];
		c_b2ContactConstraint* t_cc=t_constraints[t_i];
		for(int t_j=0;t_j<t_cc->m_pointCount;t_j=t_j+1){
			m_s_impulse->m_normalImpulses[t_j]=t_cc->m_points[t_j]->m_normalImpulse;
			m_s_impulse->m_tangentImpulses[t_j]=t_cc->m_points[t_j]->m_tangentImpulse;
		}
		m_m_listener->p_PostSolve(t_c,m_s_impulse);
	}
}
void c_b2Island::p_Solve4(c_b2TimeStep* t_timeStep,c_b2Vec2* t_gravity,bool t_allowSleep){
	int t_i=0;
	int t_j=0;
	c_b2Body* t_b=0;
	c_b2Joint* t_joint=0;
	for(int t_i2=0;t_i2<m_m_bodyCount;t_i2=t_i2+1){
		t_b=m_m_bodies[t_i2];
		if(t_b->m_m_type!=2){
			continue;
		}
		t_b->m_m_linearVelocity->m_x+=t_timeStep->m_dt*(t_gravity->m_x+t_b->m_m_invMass*t_b->m_m_force->m_x);
		t_b->m_m_linearVelocity->m_y+=t_timeStep->m_dt*(t_gravity->m_y+t_b->m_m_invMass*t_b->m_m_force->m_y);
		t_b->m_m_angularVelocity+=t_timeStep->m_dt*t_b->m_m_invI*t_b->m_m_torque;
		t_b->m_m_linearVelocity->p_Multiply(c_b2Math::m_Clamp(FLOAT(1.0)-t_timeStep->m_dt*t_b->m_m_linearDamping,FLOAT(0.0),FLOAT(1.0)));
		t_b->m_m_angularVelocity*=c_b2Math::m_Clamp(FLOAT(1.0)-t_timeStep->m_dt*t_b->m_m_angularDamping,FLOAT(0.0),FLOAT(1.0));
	}
	m_m_contactSolver->p_Initialize2(t_timeStep,m_m_contacts,m_m_contactCount,m_m_allocator);
	c_b2ContactSolver* t_contactSolver=m_m_contactSolver;
	t_contactSolver->p_InitVelocityConstraints(t_timeStep);
	for(int t_i3=0;t_i3<m_m_jointCount;t_i3=t_i3+1){
		t_joint=m_m_joints[t_i3];
		t_joint->p_InitVelocityConstraints(t_timeStep);
	}
	for(int t_i4=0;t_i4<t_timeStep->m_velocityIterations;t_i4=t_i4+1){
		for(int t_j2=0;t_j2<m_m_jointCount;t_j2=t_j2+1){
			t_joint=m_m_joints[t_j2];
			t_joint->p_SolveVelocityConstraints(t_timeStep);
		}
		t_contactSolver->p_SolveVelocityConstraints2();
	}
	for(int t_i5=0;t_i5<m_m_jointCount;t_i5=t_i5+1){
		t_joint=m_m_joints[t_i5];
		t_joint->p_FinalizeVelocityConstraints();
	}
	t_contactSolver->p_FinalizeVelocityConstraints();
	for(int t_i6=0;t_i6<m_m_bodyCount;t_i6=t_i6+1){
		t_b=m_m_bodies[t_i6];
		if(t_b->m_m_type==0){
			continue;
		}
		Float t_translationX=t_timeStep->m_dt*t_b->m_m_linearVelocity->m_x;
		Float t_translationY=t_timeStep->m_dt*t_b->m_m_linearVelocity->m_y;
		if(t_translationX*t_translationX+t_translationY*t_translationY>FLOAT(4.0)){
			t_b->m_m_linearVelocity->p_Normalize();
			t_b->m_m_linearVelocity->m_x*=FLOAT(2.0)*t_timeStep->m_inv_dt;
			t_b->m_m_linearVelocity->m_y*=FLOAT(2.0)*t_timeStep->m_inv_dt;
		}
		Float t_rotation=t_timeStep->m_dt*t_b->m_m_angularVelocity;
		if(t_rotation*t_rotation>FLOAT(2.4674010946335061)){
			if(t_b->m_m_angularVelocity<FLOAT(0.0)){
				t_b->m_m_angularVelocity=FLOAT(-1.5707963250000001)*t_timeStep->m_inv_dt;
			}else{
				t_b->m_m_angularVelocity=FLOAT(1.5707963250000001)*t_timeStep->m_inv_dt;
			}
		}
		t_b->m_m_sweep->m_c0->p_SetV(t_b->m_m_sweep->m_c);
		t_b->m_m_sweep->m_a0=t_b->m_m_sweep->m_a;
		t_b->m_m_sweep->m_c->m_x+=t_timeStep->m_dt*t_b->m_m_linearVelocity->m_x;
		t_b->m_m_sweep->m_c->m_y+=t_timeStep->m_dt*t_b->m_m_linearVelocity->m_y;
		t_b->m_m_sweep->m_a+=t_timeStep->m_dt*t_b->m_m_angularVelocity;
		t_b->p_SynchronizeTransform();
	}
	for(int t_i7=0;t_i7<t_timeStep->m_positionIterations;t_i7=t_i7+1){
		bool t_contactsOkay=t_contactSolver->p_SolvePositionConstraints(FLOAT(0.2));
		bool t_jointsOkay=true;
		for(int t_j3=0;t_j3<m_m_jointCount;t_j3=t_j3+1){
			t_joint=m_m_joints[t_j3];
			bool t_jointOkay=t_joint->p_SolvePositionConstraints(FLOAT(0.2));
			t_jointsOkay=t_jointsOkay && t_jointOkay;
		}
		if(t_contactsOkay && t_jointsOkay){
			break;
		}
	}
	p_Report(t_contactSolver->m_m_constraints);
	if(t_allowSleep){
		Float t_minSleepTime=FLOAT(3.4e38);
		Float t_linTolSqr=FLOAT(0.0001);
		Float t_angTolSqr=FLOAT(0.0012184696763622254);
		for(int t_i8=0;t_i8<m_m_bodyCount;t_i8=t_i8+1){
			t_b=m_m_bodies[t_i8];
			if(t_b->m_m_type==0){
				continue;
			}
			if((t_b->m_m_flags&4)==0){
				t_b->m_m_sleepTime=FLOAT(0.0);
				t_minSleepTime=FLOAT(0.0);
			}
			if((t_b->m_m_flags&4)==0 || t_b->m_m_angularVelocity*t_b->m_m_angularVelocity>t_angTolSqr || c_b2Math::m_Dot(t_b->m_m_linearVelocity,t_b->m_m_linearVelocity)>t_linTolSqr){
				t_b->m_m_sleepTime=FLOAT(0.0);
				t_minSleepTime=FLOAT(0.0);
			}else{
				t_b->m_m_sleepTime+=t_timeStep->m_dt;
				t_minSleepTime=c_b2Math::m_Min(t_minSleepTime,t_b->m_m_sleepTime);
			}
		}
		if(t_minSleepTime>=FLOAT(0.5)){
			for(int t_i9=0;t_i9<m_m_bodyCount;t_i9=t_i9+1){
				t_b=m_m_bodies[t_i9];
				t_b->p_SetAwake(false);
			}
		}
	}
}
void c_b2Island::p_SolveTOI(c_b2TimeStep* t_subStep){
	int t_i=0;
	int t_j=0;
	m_m_contactSolver->p_Initialize2(t_subStep,m_m_contacts,m_m_contactCount,m_m_allocator);
	c_b2ContactSolver* t_contactSolver=m_m_contactSolver;
	for(int t_i2=0;t_i2<m_m_jointCount;t_i2=t_i2+1){
		m_m_joints[t_i2]->p_InitVelocityConstraints(t_subStep);
	}
	for(int t_i3=0;t_i3<t_subStep->m_velocityIterations;t_i3=t_i3+1){
		t_contactSolver->p_SolveVelocityConstraints2();
		for(int t_j2=0;t_j2<m_m_jointCount;t_j2=t_j2+1){
			m_m_joints[t_j2]->p_SolveVelocityConstraints(t_subStep);
		}
	}
	for(int t_i4=0;t_i4<m_m_bodyCount;t_i4=t_i4+1){
		c_b2Body* t_b=m_m_bodies[t_i4];
		if(t_b->m_m_type==0){
			continue;
		}
		Float t_translationX=t_subStep->m_dt*t_b->m_m_linearVelocity->m_x;
		Float t_translationY=t_subStep->m_dt*t_b->m_m_linearVelocity->m_y;
		if(t_translationX*t_translationX+t_translationY*t_translationY>FLOAT(4.0)){
			t_b->m_m_linearVelocity->p_Normalize();
			t_b->m_m_linearVelocity->m_x*=FLOAT(2.0)*t_subStep->m_inv_dt;
			t_b->m_m_linearVelocity->m_y*=FLOAT(2.0)*t_subStep->m_inv_dt;
		}
		Float t_rotation=t_subStep->m_dt*t_b->m_m_angularVelocity;
		if(t_rotation*t_rotation>FLOAT(2.4674010946335061)){
			if(t_b->m_m_angularVelocity<FLOAT(0.0)){
				t_b->m_m_angularVelocity=FLOAT(-1.5707963250000001)*t_subStep->m_inv_dt;
			}else{
				t_b->m_m_angularVelocity=FLOAT(1.5707963250000001)*t_subStep->m_inv_dt;
			}
		}
		t_b->m_m_sweep->m_c0->p_SetV(t_b->m_m_sweep->m_c);
		t_b->m_m_sweep->m_a0=t_b->m_m_sweep->m_a;
		t_b->m_m_sweep->m_c->m_x+=t_subStep->m_dt*t_b->m_m_linearVelocity->m_x;
		t_b->m_m_sweep->m_c->m_y+=t_subStep->m_dt*t_b->m_m_linearVelocity->m_y;
		t_b->m_m_sweep->m_a+=t_subStep->m_dt*t_b->m_m_angularVelocity;
		t_b->p_SynchronizeTransform();
	}
	Float t_k_toiBaumgarte=FLOAT(0.75);
	for(int t_i5=0;t_i5<t_subStep->m_positionIterations;t_i5=t_i5+1){
		bool t_contactsOkay=t_contactSolver->p_SolvePositionConstraints(t_k_toiBaumgarte);
		bool t_jointsOkay=true;
		for(int t_j3=0;t_j3<m_m_jointCount;t_j3=t_j3+1){
			bool t_jointOkay=m_m_joints[t_j3]->p_SolvePositionConstraints(FLOAT(0.2));
			t_jointsOkay=t_jointsOkay && t_jointOkay;
		}
		if(t_contactsOkay && t_jointsOkay){
			break;
		}
	}
	p_Report(t_contactSolver->m_m_constraints);
}
void c_b2Island::mark(){
	Object::mark();
	gc_mark_q(m_m_bodies);
	gc_mark_q(m_m_contacts);
	gc_mark_q(m_m_joints);
	gc_mark_q(m_m_allocator);
	gc_mark_q(m_m_listener);
	gc_mark_q(m_m_contactSolver);
}
c_b2ContactSolver::c_b2ContactSolver(){
	m_constraintCapacity=1000;
	m_m_constraints=Array<c_b2ContactConstraint* >();
	m_m_step=(new c_b2TimeStep)->m_new();
	m_m_allocator=0;
	m_m_constraintCount=0;
}
c_b2ContactSolver* c_b2ContactSolver::m_new(){
	gc_assign(m_m_constraints,Array<c_b2ContactConstraint* >(m_constraintCapacity));
	for(int t_i=0;t_i<m_constraintCapacity;t_i=t_i+1){
		gc_assign(m_m_constraints[t_i],(new c_b2ContactConstraint)->m_new());
	}
	return this;
}
c_b2WorldManifold* c_b2ContactSolver::m_s_worldManifold;
void c_b2ContactSolver::p_Initialize2(c_b2TimeStep* t_timeStep,Array<c_b2Contact* > t_contacts,int t_contactCount,Object* t_allocator){
	c_b2Contact* t_contact=0;
	m_m_step->p_Set9(t_timeStep);
	gc_assign(m_m_allocator,t_allocator);
	int t_i=0;
	c_b2Vec2* t_tVec=0;
	c_b2Mat22* t_tMat=0;
	m_m_constraintCount=t_contactCount;
	if(m_m_constraintCount>m_constraintCapacity){
		gc_assign(m_m_constraints,m_m_constraints.Resize(m_m_constraintCount));
		for(int t_i2=m_constraintCapacity;t_i2<m_m_constraintCount;t_i2=t_i2+1){
			gc_assign(m_m_constraints[t_i2],(new c_b2ContactConstraint)->m_new());
		}
		m_constraintCapacity=m_m_constraintCount;
	}
	for(int t_i3=0;t_i3<t_contactCount;t_i3=t_i3+1){
		t_contact=t_contacts[t_i3];
		c_b2Fixture* t_fixtureA=t_contact->m_m_fixtureA;
		c_b2Fixture* t_fixtureB=t_contact->m_m_fixtureB;
		c_b2Shape* t_shapeA=t_fixtureA->m_m_shape;
		c_b2Shape* t_shapeB=t_fixtureB->m_m_shape;
		Float t_radiusA=t_shapeA->m_m_radius;
		Float t_radiusB=t_shapeB->m_m_radius;
		c_b2Body* t_bodyA=t_fixtureA->m_m_body;
		c_b2Body* t_bodyB=t_fixtureB->m_m_body;
		c_b2Manifold* t_manifold=t_contact->p_GetManifold();
		Float t_friction=c_b2Settings::m_B2MixFriction(t_fixtureA->p_GetFriction(),t_fixtureB->p_GetFriction());
		Float t_restitution=c_b2Settings::m_B2MixRestitution(t_fixtureA->p_GetRestitution(),t_fixtureB->p_GetRestitution());
		Float t_vAX=t_bodyA->m_m_linearVelocity->m_x;
		Float t_vAY=t_bodyA->m_m_linearVelocity->m_y;
		Float t_vBX=t_bodyB->m_m_linearVelocity->m_x;
		Float t_vBY=t_bodyB->m_m_linearVelocity->m_y;
		Float t_wA=t_bodyA->m_m_angularVelocity;
		Float t_wB=t_bodyB->m_m_angularVelocity;
		m_s_worldManifold->p_Initialize3(t_manifold,t_bodyA->m_m_xf,t_radiusA,t_bodyB->m_m_xf,t_radiusB);
		Float t_normalX=m_s_worldManifold->m_m_normal->m_x;
		Float t_normalY=m_s_worldManifold->m_m_normal->m_y;
		c_b2ContactConstraint* t_cc=m_m_constraints[t_i3];
		gc_assign(t_cc->m_bodyA,t_bodyA);
		gc_assign(t_cc->m_bodyB,t_bodyB);
		gc_assign(t_cc->m_manifold,t_manifold);
		t_cc->m_normal->m_x=t_normalX;
		t_cc->m_normal->m_y=t_normalY;
		t_cc->m_pointCount=t_manifold->m_m_pointCount;
		t_cc->m_friction=t_friction;
		t_cc->m_restitution=t_restitution;
		t_cc->m_localPlaneNormal->m_x=t_manifold->m_m_localPlaneNormal->m_x;
		t_cc->m_localPlaneNormal->m_y=t_manifold->m_m_localPlaneNormal->m_y;
		t_cc->m_localPoint->m_x=t_manifold->m_m_localPoint->m_x;
		t_cc->m_localPoint->m_y=t_manifold->m_m_localPoint->m_y;
		t_cc->m_radius=t_radiusA+t_radiusB;
		t_cc->m_type=t_manifold->m_m_type;
		for(int t_k=0;t_k<t_cc->m_pointCount;t_k=t_k+1){
			c_b2ManifoldPoint* t_cp=t_manifold->m_m_points[t_k];
			c_b2ContactConstraintPoint* t_ccp=t_cc->m_points[t_k];
			t_ccp->m_normalImpulse=t_cp->m_m_normalImpulse;
			t_ccp->m_tangentImpulse=t_cp->m_m_tangentImpulse;
			t_ccp->m_localPoint->m_x=t_cp->m_m_localPoint->m_x;
			t_ccp->m_localPoint->m_y=t_cp->m_m_localPoint->m_y;
			t_ccp->m_rA->m_x=m_s_worldManifold->m_m_points[t_k]->m_x-t_bodyA->m_m_sweep->m_c->m_x;
			Float t_rAX=t_ccp->m_rA->m_x;
			t_ccp->m_rA->m_y=m_s_worldManifold->m_m_points[t_k]->m_y-t_bodyA->m_m_sweep->m_c->m_y;
			Float t_rAY=t_ccp->m_rA->m_y;
			t_ccp->m_rB->m_x=m_s_worldManifold->m_m_points[t_k]->m_x-t_bodyB->m_m_sweep->m_c->m_x;
			Float t_rBX=t_ccp->m_rB->m_x;
			t_ccp->m_rB->m_y=m_s_worldManifold->m_m_points[t_k]->m_y-t_bodyB->m_m_sweep->m_c->m_y;
			Float t_rBY=t_ccp->m_rB->m_y;
			Float t_rnA=t_rAX*t_normalY-t_rAY*t_normalX;
			Float t_rnB=t_rBX*t_normalY-t_rBY*t_normalX;
			t_rnA*=t_rnA;
			t_rnB*=t_rnB;
			Float t_kNormal=t_bodyA->m_m_invMass+t_bodyB->m_m_invMass+t_bodyA->m_m_invI*t_rnA+t_bodyB->m_m_invI*t_rnB;
			t_ccp->m_normalMass=FLOAT(1.0)/t_kNormal;
			Float t_kEqualized=t_bodyA->m_m_mass*t_bodyA->m_m_invMass+t_bodyB->m_m_mass*t_bodyB->m_m_invMass;
			t_kEqualized+=t_bodyA->m_m_mass*t_bodyA->m_m_invI*t_rnA+t_bodyB->m_m_mass*t_bodyB->m_m_invI*t_rnB;
			t_ccp->m_equalizedMass=FLOAT(1.0)/t_kEqualized;
			Float t_tangentX=t_normalY;
			Float t_tangentY=-t_normalX;
			Float t_rtA=t_rAX*t_tangentY-t_rAY*t_tangentX;
			Float t_rtB=t_rBX*t_tangentY-t_rBY*t_tangentX;
			t_rtA*=t_rtA;
			t_rtB*=t_rtB;
			Float t_kTangent=t_bodyA->m_m_invMass+t_bodyB->m_m_invMass+t_bodyA->m_m_invI*t_rtA+t_bodyB->m_m_invI*t_rtB;
			t_ccp->m_tangentMass=FLOAT(1.0)/t_kTangent;
			t_ccp->m_velocityBias=FLOAT(0.0);
			Float t_tX=t_vBX+t_wB*-t_rBY-t_vAX-t_wA*-t_rAY;
			Float t_tY=t_vBY+t_wB*t_rBX-t_vAY-t_wA*t_rAX;
			Float t_vRel=t_cc->m_normal->m_x*t_tX+t_cc->m_normal->m_y*t_tY;
			if(t_vRel<FLOAT(-1.0)){
				t_ccp->m_velocityBias+=-t_cc->m_restitution*t_vRel;
			}
		}
		if(t_cc->m_pointCount==2){
			c_b2ContactConstraintPoint* t_ccp1=t_cc->m_points[0];
			c_b2ContactConstraintPoint* t_ccp2=t_cc->m_points[1];
			Float t_invMassA=t_bodyA->m_m_invMass;
			Float t_invIA=t_bodyA->m_m_invI;
			Float t_invMassB=t_bodyB->m_m_invMass;
			Float t_invIB=t_bodyB->m_m_invI;
			Float t_rn1A=t_ccp1->m_rA->m_x*t_normalY-t_ccp1->m_rA->m_y*t_normalX;
			Float t_rn1B=t_ccp1->m_rB->m_x*t_normalY-t_ccp1->m_rB->m_y*t_normalX;
			Float t_rn2A=t_ccp2->m_rA->m_x*t_normalY-t_ccp2->m_rA->m_y*t_normalX;
			Float t_rn2B=t_ccp2->m_rB->m_x*t_normalY-t_ccp2->m_rB->m_y*t_normalX;
			Float t_k11=t_invMassA+t_invMassB+t_invIA*t_rn1A*t_rn1A+t_invIB*t_rn1B*t_rn1B;
			Float t_k22=t_invMassA+t_invMassB+t_invIA*t_rn2A*t_rn2A+t_invIB*t_rn2B*t_rn2B;
			Float t_k12=t_invMassA+t_invMassB+t_invIA*t_rn1A*t_rn2A+t_invIB*t_rn1B*t_rn2B;
			Float t_k_maxConditionNumber=FLOAT(100.0);
			if(t_k11*t_k11<t_k_maxConditionNumber*(t_k11*t_k22-t_k12*t_k12)){
				t_cc->m_K->m_col1->m_x=t_k11;
				t_cc->m_K->m_col1->m_y=t_k12;
				t_cc->m_K->m_col2->m_x=t_k12;
				t_cc->m_K->m_col2->m_y=t_k22;
				t_cc->m_K->p_GetInverse(t_cc->m_normalMass);
			}else{
				t_cc->m_pointCount=1;
			}
		}
	}
}
void c_b2ContactSolver::p_InitVelocityConstraints(c_b2TimeStep* t_timeStep){
	c_b2Vec2* t_tVec=0;
	c_b2Vec2* t_tVec2=0;
	c_b2Mat22* t_tMat=0;
	for(int t_i=0;t_i<m_m_constraintCount;t_i=t_i+1){
		c_b2ContactConstraint* t_c=m_m_constraints[t_i];
		c_b2Body* t_bodyA=t_c->m_bodyA;
		c_b2Body* t_bodyB=t_c->m_bodyB;
		Float t_invMassA=t_bodyA->m_m_invMass;
		Float t_invIA=t_bodyA->m_m_invI;
		Float t_invMassB=t_bodyB->m_m_invMass;
		Float t_invIB=t_bodyB->m_m_invI;
		Float t_normalX=t_c->m_normal->m_x;
		Float t_normalY=t_c->m_normal->m_y;
		Float t_tangentX=t_normalY;
		Float t_tangentY=-t_normalX;
		Float t_tX=FLOAT(.0);
		int t_j=0;
		int t_tCount=0;
		if(t_timeStep->m_warmStarting){
			t_tCount=t_c->m_pointCount;
			for(int t_j2=0;t_j2<t_tCount;t_j2=t_j2+1){
				c_b2ContactConstraintPoint* t_ccp=t_c->m_points[t_j2];
				t_ccp->m_normalImpulse*=t_timeStep->m_dtRatio;
				t_ccp->m_tangentImpulse*=t_timeStep->m_dtRatio;
				Float t_PX=t_ccp->m_normalImpulse*t_normalX+t_ccp->m_tangentImpulse*t_tangentX;
				Float t_PY=t_ccp->m_normalImpulse*t_normalY+t_ccp->m_tangentImpulse*t_tangentY;
				t_bodyA->m_m_angularVelocity-=t_invIA*(t_ccp->m_rA->m_x*t_PY-t_ccp->m_rA->m_y*t_PX);
				t_bodyA->m_m_linearVelocity->m_x-=t_invMassA*t_PX;
				t_bodyA->m_m_linearVelocity->m_y-=t_invMassA*t_PY;
				t_bodyB->m_m_angularVelocity+=t_invIB*(t_ccp->m_rB->m_x*t_PY-t_ccp->m_rB->m_y*t_PX);
				t_bodyB->m_m_linearVelocity->m_x+=t_invMassB*t_PX;
				t_bodyB->m_m_linearVelocity->m_y+=t_invMassB*t_PY;
			}
		}else{
			t_tCount=t_c->m_pointCount;
			for(int t_j3=0;t_j3<t_tCount;t_j3=t_j3+1){
				c_b2ContactConstraintPoint* t_ccp2=t_c->m_points[t_j3];
				t_ccp2->m_normalImpulse=FLOAT(0.0);
				t_ccp2->m_tangentImpulse=FLOAT(0.0);
			}
		}
	}
}
void c_b2ContactSolver::p_SolveVelocityConstraints2(){
	int t_j=0;
	c_b2ContactConstraintPoint* t_ccp=0;
	Float t_rAX=FLOAT(.0);
	Float t_rAY=FLOAT(.0);
	Float t_rBX=FLOAT(.0);
	Float t_rBY=FLOAT(.0);
	Float t_dvX=FLOAT(.0);
	Float t_dvY=FLOAT(.0);
	Float t_vn=FLOAT(.0);
	Float t_vt=FLOAT(.0);
	Float t_lambda=FLOAT(.0);
	Float t_maxFriction=FLOAT(.0);
	Float t_newImpulse=FLOAT(.0);
	Float t_PX=FLOAT(.0);
	Float t_PY=FLOAT(.0);
	Float t_dX=FLOAT(.0);
	Float t_dY=FLOAT(.0);
	Float t_P1X=FLOAT(.0);
	Float t_P1Y=FLOAT(.0);
	Float t_P2X=FLOAT(.0);
	Float t_P2Y=FLOAT(.0);
	c_b2Mat22* t_tMat=0;
	c_b2Vec2* t_tVec=0;
	for(int t_i=0;t_i<m_m_constraintCount;t_i=t_i+1){
		c_b2ContactConstraint* t_c=m_m_constraints[t_i];
		c_b2Body* t_bodyA=t_c->m_bodyA;
		c_b2Body* t_bodyB=t_c->m_bodyB;
		Float t_wA=t_bodyA->m_m_angularVelocity;
		Float t_wB=t_bodyB->m_m_angularVelocity;
		c_b2Vec2* t_vA=t_bodyA->m_m_linearVelocity;
		c_b2Vec2* t_vB=t_bodyB->m_m_linearVelocity;
		Float t_invMassA=t_bodyA->m_m_invMass;
		Float t_invIA=t_bodyA->m_m_invI;
		Float t_invMassB=t_bodyB->m_m_invMass;
		Float t_invIB=t_bodyB->m_m_invI;
		Float t_normalX=t_c->m_normal->m_x;
		Float t_normalY=t_c->m_normal->m_y;
		Float t_tangentX=t_normalY;
		Float t_tangentY=-t_normalX;
		Float t_friction=t_c->m_friction;
		Float t_tX=FLOAT(.0);
		for(int t_j2=0;t_j2<t_c->m_pointCount;t_j2=t_j2+1){
			t_ccp=t_c->m_points[t_j2];
			t_dvX=t_vB->m_x-t_wB*t_ccp->m_rB->m_y-t_vA->m_x+t_wA*t_ccp->m_rA->m_y;
			t_dvY=t_vB->m_y+t_wB*t_ccp->m_rB->m_x-t_vA->m_y-t_wA*t_ccp->m_rA->m_x;
			t_vt=t_dvX*t_tangentX+t_dvY*t_tangentY;
			t_lambda=t_ccp->m_tangentMass*-t_vt;
			t_maxFriction=t_friction*t_ccp->m_normalImpulse;
			t_newImpulse=t_ccp->m_tangentImpulse+t_lambda;
			if(t_newImpulse<-t_maxFriction){
				t_newImpulse=-t_maxFriction;
			}else{
				if(t_newImpulse>t_maxFriction){
					t_newImpulse=t_maxFriction;
				}
			}
			t_lambda=t_newImpulse-t_ccp->m_tangentImpulse;
			t_PX=t_lambda*t_tangentX;
			t_PY=t_lambda*t_tangentY;
			t_vA->m_x-=t_invMassA*t_PX;
			t_vA->m_y-=t_invMassA*t_PY;
			t_wA-=t_invIA*(t_ccp->m_rA->m_x*t_PY-t_ccp->m_rA->m_y*t_PX);
			t_vB->m_x+=t_invMassB*t_PX;
			t_vB->m_y+=t_invMassB*t_PY;
			t_wB+=t_invIB*(t_ccp->m_rB->m_x*t_PY-t_ccp->m_rB->m_y*t_PX);
			t_ccp->m_tangentImpulse=t_newImpulse;
		}
		int t_tCount=t_c->m_pointCount;
		if(t_c->m_pointCount==1){
			t_ccp=t_c->m_points[0];
			c_b2Vec2* t_ccpRa=t_ccp->m_rA;
			c_b2Vec2* t_ccpRb=t_ccp->m_rB;
			t_dvX=t_vB->m_x+t_wB*-t_ccpRb->m_y-t_vA->m_x-t_wA*-t_ccpRa->m_y;
			t_dvY=t_vB->m_y+t_wB*t_ccpRb->m_x-t_vA->m_y-t_wA*t_ccpRa->m_x;
			t_vn=t_dvX*t_normalX+t_dvY*t_normalY;
			t_lambda=-t_ccp->m_normalMass*(t_vn-t_ccp->m_velocityBias);
			t_newImpulse=t_ccp->m_normalImpulse+t_lambda;
			if(t_newImpulse<FLOAT(0.0)){
				t_newImpulse=FLOAT(0.0);
			}
			t_lambda=t_newImpulse-t_ccp->m_normalImpulse;
			t_PX=t_lambda*t_normalX;
			t_PY=t_lambda*t_normalY;
			t_vA->m_x-=t_invMassA*t_PX;
			t_vA->m_y-=t_invMassA*t_PY;
			t_wA-=t_invIA*(t_ccpRa->m_x*t_PY-t_ccpRa->m_y*t_PX);
			t_vB->m_x+=t_invMassB*t_PX;
			t_vB->m_y+=t_invMassB*t_PY;
			t_wB+=t_invIB*(t_ccpRb->m_x*t_PY-t_ccpRb->m_y*t_PX);
			t_ccp->m_normalImpulse=t_newImpulse;
		}else{
			c_b2ContactConstraintPoint* t_cp1=t_c->m_points[0];
			c_b2Vec2* t_cp1rA=t_cp1->m_rA;
			c_b2Vec2* t_cp1rB=t_cp1->m_rB;
			c_b2ContactConstraintPoint* t_cp2=t_c->m_points[1];
			c_b2Vec2* t_cp2rA=t_cp2->m_rA;
			c_b2Vec2* t_cp2rB=t_cp2->m_rB;
			Float t_aX=t_cp1->m_normalImpulse;
			Float t_aY=t_cp2->m_normalImpulse;
			Float t_dv1X=t_vB->m_x-t_wB*t_cp1rB->m_y-t_vA->m_x+t_wA*t_cp1rA->m_y;
			Float t_dv1Y=t_vB->m_y+t_wB*t_cp1rB->m_x-t_vA->m_y-t_wA*t_cp1rA->m_x;
			Float t_dv2X=t_vB->m_x-t_wB*t_cp2rB->m_y-t_vA->m_x+t_wA*t_cp2rA->m_y;
			Float t_dv2Y=t_vB->m_y+t_wB*t_cp2rB->m_x-t_vA->m_y-t_wA*t_cp2rA->m_x;
			Float t_vn1=t_dv1X*t_normalX+t_dv1Y*t_normalY;
			Float t_vn2=t_dv2X*t_normalX+t_dv2Y*t_normalY;
			Float t_bX=t_vn1-t_cp1->m_velocityBias;
			Float t_bY=t_vn2-t_cp2->m_velocityBias;
			t_tMat=t_c->m_K;
			t_bX-=t_tMat->m_col1->m_x*t_aX+t_tMat->m_col2->m_x*t_aY;
			t_bY-=t_tMat->m_col1->m_y*t_aX+t_tMat->m_col2->m_y*t_aY;
			Float t_k_errorTol=FLOAT(0.001);
			while(true){
				t_tMat=t_c->m_normalMass;
				Float t_xX=-(t_tMat->m_col1->m_x*t_bX+t_tMat->m_col2->m_x*t_bY);
				Float t_xY=-(t_tMat->m_col1->m_y*t_bX+t_tMat->m_col2->m_y*t_bY);
				if(t_xX>=FLOAT(0.0) && t_xY>=FLOAT(0.0)){
					t_dX=t_xX-t_aX;
					t_dY=t_xY-t_aY;
					t_P1X=t_dX*t_normalX;
					t_P1Y=t_dX*t_normalY;
					t_P2X=t_dY*t_normalX;
					t_P2Y=t_dY*t_normalY;
					t_vA->m_x-=t_invMassA*(t_P1X+t_P2X);
					t_vA->m_y-=t_invMassA*(t_P1Y+t_P2Y);
					t_wA-=t_invIA*(t_cp1rA->m_x*t_P1Y-t_cp1rA->m_y*t_P1X+t_cp2rA->m_x*t_P2Y-t_cp2rA->m_y*t_P2X);
					t_vB->m_x+=t_invMassB*(t_P1X+t_P2X);
					t_vB->m_y+=t_invMassB*(t_P1Y+t_P2Y);
					t_wB+=t_invIB*(t_cp1rB->m_x*t_P1Y-t_cp1rB->m_y*t_P1X+t_cp2rB->m_x*t_P2Y-t_cp2rB->m_y*t_P2X);
					t_cp1->m_normalImpulse=t_xX;
					t_cp2->m_normalImpulse=t_xY;
					break;
				}
				t_xX=-t_cp1->m_normalMass*t_bX;
				t_xY=FLOAT(0.0);
				t_vn1=FLOAT(0.0);
				t_vn2=t_c->m_K->m_col1->m_y*t_xX+t_bY;
				if(t_xX>=FLOAT(0.0) && t_vn2>=FLOAT(0.0)){
					t_dX=t_xX-t_aX;
					t_dY=t_xY-t_aY;
					t_P1X=t_dX*t_normalX;
					t_P1Y=t_dX*t_normalY;
					t_P2X=t_dY*t_normalX;
					t_P2Y=t_dY*t_normalY;
					t_vA->m_x-=t_invMassA*(t_P1X+t_P2X);
					t_vA->m_y-=t_invMassA*(t_P1Y+t_P2Y);
					t_wA-=t_invIA*(t_cp1rA->m_x*t_P1Y-t_cp1rA->m_y*t_P1X+t_cp2rA->m_x*t_P2Y-t_cp2rA->m_y*t_P2X);
					t_vB->m_x+=t_invMassB*(t_P1X+t_P2X);
					t_vB->m_y+=t_invMassB*(t_P1Y+t_P2Y);
					t_wB+=t_invIB*(t_cp1rB->m_x*t_P1Y-t_cp1rB->m_y*t_P1X+t_cp2rB->m_x*t_P2Y-t_cp2rB->m_y*t_P2X);
					t_cp1->m_normalImpulse=t_xX;
					t_cp2->m_normalImpulse=t_xY;
					break;
				}
				t_xX=FLOAT(0.0);
				t_xY=-t_cp2->m_normalMass*t_bY;
				t_vn1=t_c->m_K->m_col2->m_x*t_xY+t_bX;
				t_vn2=FLOAT(0.0);
				if(t_xY>=FLOAT(0.0) && t_vn1>=FLOAT(0.0)){
					t_dX=t_xX-t_aX;
					t_dY=t_xY-t_aY;
					t_P1X=t_dX*t_normalX;
					t_P1Y=t_dX*t_normalY;
					t_P2X=t_dY*t_normalX;
					t_P2Y=t_dY*t_normalY;
					t_vA->m_x-=t_invMassA*(t_P1X+t_P2X);
					t_vA->m_y-=t_invMassA*(t_P1Y+t_P2Y);
					t_wA-=t_invIA*(t_cp1rA->m_x*t_P1Y-t_cp1rA->m_y*t_P1X+t_cp2rA->m_x*t_P2Y-t_cp2rA->m_y*t_P2X);
					t_vB->m_x+=t_invMassB*(t_P1X+t_P2X);
					t_vB->m_y+=t_invMassB*(t_P1Y+t_P2Y);
					t_wB+=t_invIB*(t_cp1rB->m_x*t_P1Y-t_cp1rB->m_y*t_P1X+t_cp2rB->m_x*t_P2Y-t_cp2rB->m_y*t_P2X);
					t_cp1->m_normalImpulse=t_xX;
					t_cp2->m_normalImpulse=t_xY;
					break;
				}
				t_xX=FLOAT(0.0);
				t_xY=FLOAT(0.0);
				t_vn1=t_bX;
				t_vn2=t_bY;
				if(t_vn1>=FLOAT(0.0) && t_vn2>=FLOAT(0.0)){
					t_dX=t_xX-t_aX;
					t_dY=t_xY-t_aY;
					t_P1X=t_dX*t_normalX;
					t_P1Y=t_dX*t_normalY;
					t_P2X=t_dY*t_normalX;
					t_P2Y=t_dY*t_normalY;
					t_vA->m_x-=t_invMassA*(t_P1X+t_P2X);
					t_vA->m_y-=t_invMassA*(t_P1Y+t_P2Y);
					t_wA-=t_invIA*(t_cp1rA->m_x*t_P1Y-t_cp1rA->m_y*t_P1X+t_cp2rA->m_x*t_P2Y-t_cp2rA->m_y*t_P2X);
					t_vB->m_x+=t_invMassB*(t_P1X+t_P2X);
					t_vB->m_y+=t_invMassB*(t_P1Y+t_P2Y);
					t_wB+=t_invIB*(t_cp1rB->m_x*t_P1Y-t_cp1rB->m_y*t_P1X+t_cp2rB->m_x*t_P2Y-t_cp2rB->m_y*t_P2X);
					t_cp1->m_normalImpulse=t_xX;
					t_cp2->m_normalImpulse=t_xY;
					break;
				}
				break;
			}
		}
		t_bodyA->m_m_angularVelocity=t_wA;
		t_bodyB->m_m_angularVelocity=t_wB;
	}
}
void c_b2ContactSolver::p_FinalizeVelocityConstraints(){
	for(int t_i=0;t_i<m_m_constraintCount;t_i=t_i+1){
		c_b2ContactConstraint* t_c=m_m_constraints[t_i];
		c_b2Manifold* t_m=t_c->m_manifold;
		for(int t_j=0;t_j<t_c->m_pointCount;t_j=t_j+1){
			c_b2ManifoldPoint* t_point1=t_m->m_m_points[t_j];
			c_b2ContactConstraintPoint* t_point2=t_c->m_points[t_j];
			t_point1->m_m_normalImpulse=t_point2->m_normalImpulse;
			t_point1->m_m_tangentImpulse=t_point2->m_tangentImpulse;
		}
	}
}
c_b2PositionSolverManifold* c_b2ContactSolver::m_s_psm;
bool c_b2ContactSolver::p_SolvePositionConstraints(Float t_baumgarte){
	Float t_minSeparation=FLOAT(0.0);
	for(int t_i=0;t_i<m_m_constraintCount;t_i=t_i+1){
		c_b2ContactConstraint* t_c=m_m_constraints[t_i];
		c_b2Body* t_bodyA=t_c->m_bodyA;
		c_b2Body* t_bodyB=t_c->m_bodyB;
		Float t_invMassA=t_bodyA->m_m_mass*t_bodyA->m_m_invMass;
		Float t_invIA=t_bodyA->m_m_mass*t_bodyA->m_m_invI;
		Float t_invMassB=t_bodyB->m_m_mass*t_bodyB->m_m_invMass;
		Float t_invIB=t_bodyB->m_m_mass*t_bodyB->m_m_invI;
		m_s_psm->p_Initialize4(t_c);
		c_b2Vec2* t_normal=m_s_psm->m_m_normal;
		c_b2Sweep* t_ba_sweep=t_bodyA->m_m_sweep;
		c_b2Vec2* t_ba_sweepc=t_ba_sweep->m_c;
		c_b2Transform* t_ba_xf=t_bodyA->m_m_xf;
		c_b2Vec2* t_ba_xfPos=t_ba_xf->m_position;
		c_b2Mat22* t_ba_tMat=t_ba_xf->m_R;
		c_b2Vec2* t_ba_tMat_col1=t_ba_tMat->m_col1;
		c_b2Vec2* t_ba_tMat_col2=t_ba_tMat->m_col2;
		c_b2Vec2* t_ba_tVec=t_ba_sweep->m_localCenter;
		c_b2Sweep* t_bb_sweep=t_bodyB->m_m_sweep;
		c_b2Vec2* t_bb_sweepc=t_bb_sweep->m_c;
		c_b2Transform* t_bb_xf=t_bodyB->m_m_xf;
		c_b2Vec2* t_bb_xfPos=t_bb_xf->m_position;
		c_b2Mat22* t_bb_tMat=t_bb_xf->m_R;
		c_b2Vec2* t_bb_tMat_col1=t_bb_tMat->m_col1;
		c_b2Vec2* t_bb_tMat_col2=t_bb_tMat->m_col2;
		c_b2Vec2* t_bb_tVec=t_bb_sweep->m_localCenter;
		for(int t_j=0;t_j<t_c->m_pointCount;t_j=t_j+1){
			c_b2ContactConstraintPoint* t_ccp=t_c->m_points[t_j];
			c_b2Vec2* t_point=m_s_psm->m_m_points[t_j];
			Float t_separation=m_s_psm->m_m_separations[t_j];
			Float t_rAX=t_point->m_x-t_ba_sweepc->m_x;
			Float t_rAY=t_point->m_y-t_ba_sweepc->m_y;
			Float t_rBX=t_point->m_x-t_bb_sweepc->m_x;
			Float t_rBY=t_point->m_y-t_bb_sweepc->m_y;
			if(t_minSeparation<t_separation){
				t_minSeparation=t_minSeparation;
			}else{
				t_minSeparation=t_separation;
			}
			Float t_C2=t_baumgarte*(t_separation+FLOAT(0.005));
			if(t_C2<FLOAT(-0.2)){
				t_C2=FLOAT(-0.2);
			}else{
				if(t_C2>FLOAT(0.0)){
					t_C2=FLOAT(0.0);
				}
			}
			Float t_impulse=-t_ccp->m_equalizedMass*t_C2;
			Float t_PX=t_impulse*t_normal->m_x;
			Float t_PY=t_impulse*t_normal->m_y;
			t_ba_sweepc->m_x-=t_invMassA*t_PX;
			t_ba_sweepc->m_y-=t_invMassA*t_PY;
			t_ba_sweep->m_a-=t_invIA*(t_rAX*t_PY-t_rAY*t_PX);
			Float t_c3=(Float)cos(t_ba_sweep->m_a);
			Float t_s=(Float)sin(t_ba_sweep->m_a);
			t_ba_tMat_col1->m_x=t_c3;
			t_ba_tMat_col2->m_x=-t_s;
			t_ba_tMat_col1->m_y=t_s;
			t_ba_tMat_col2->m_y=t_c3;
			t_ba_xfPos->m_x=t_ba_sweepc->m_x-(t_ba_tMat_col1->m_x*t_ba_tVec->m_x+t_ba_tMat_col2->m_x*t_ba_tVec->m_y);
			t_ba_xfPos->m_y=t_ba_sweepc->m_y-(t_ba_tMat_col1->m_y*t_ba_tVec->m_x+t_ba_tMat_col2->m_y*t_ba_tVec->m_y);
			t_bb_sweepc->m_x+=t_invMassB*t_PX;
			t_bb_sweepc->m_y+=t_invMassB*t_PY;
			t_bb_sweep->m_a+=t_invIB*(t_rBX*t_PY-t_rBY*t_PX);
			t_c3=(Float)cos(t_bb_sweep->m_a);
			t_s=(Float)sin(t_bb_sweep->m_a);
			t_bb_tMat_col1->m_x=t_c3;
			t_bb_tMat_col2->m_x=-t_s;
			t_bb_tMat_col1->m_y=t_s;
			t_bb_tMat_col2->m_y=t_c3;
			t_bb_xfPos->m_x=t_bb_sweepc->m_x-(t_bb_tMat_col1->m_x*t_bb_tVec->m_x+t_bb_tMat_col2->m_x*t_bb_tVec->m_y);
			t_bb_xfPos->m_y=t_bb_sweepc->m_y-(t_bb_tMat_col1->m_y*t_bb_tVec->m_x+t_bb_tMat_col2->m_y*t_bb_tVec->m_y);
		}
	}
	return t_minSeparation>FLOAT(-0.0074999999999999997);
}
void c_b2ContactSolver::mark(){
	Object::mark();
	gc_mark_q(m_m_constraints);
	gc_mark_q(m_m_step);
	gc_mark_q(m_m_allocator);
}
c_b2ContactConstraint::c_b2ContactConstraint(){
	m_points=Array<c_b2ContactConstraintPoint* >();
	m_bodyA=0;
	m_bodyB=0;
	m_manifold=0;
	m_normal=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_pointCount=0;
	m_friction=FLOAT(.0);
	m_restitution=FLOAT(.0);
	m_localPlaneNormal=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_localPoint=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_radius=FLOAT(.0);
	m_type=0;
	m_K=(new c_b2Mat22)->m_new();
	m_normalMass=(new c_b2Mat22)->m_new();
}
c_b2ContactConstraint* c_b2ContactConstraint::m_new(){
	gc_assign(m_points,Array<c_b2ContactConstraintPoint* >(2));
	for(int t_i=0;t_i<2;t_i=t_i+1){
		gc_assign(m_points[t_i],(new c_b2ContactConstraintPoint)->m_new());
	}
	return this;
}
void c_b2ContactConstraint::mark(){
	Object::mark();
	gc_mark_q(m_points);
	gc_mark_q(m_bodyA);
	gc_mark_q(m_bodyB);
	gc_mark_q(m_manifold);
	gc_mark_q(m_normal);
	gc_mark_q(m_localPlaneNormal);
	gc_mark_q(m_localPoint);
	gc_mark_q(m_K);
	gc_mark_q(m_normalMass);
}
c_b2ContactConstraintPoint::c_b2ContactConstraintPoint(){
	m_normalImpulse=FLOAT(.0);
	m_tangentImpulse=FLOAT(.0);
	m_localPoint=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_rA=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_rB=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_normalMass=FLOAT(.0);
	m_equalizedMass=FLOAT(.0);
	m_tangentMass=FLOAT(.0);
	m_velocityBias=FLOAT(.0);
}
c_b2ContactConstraintPoint* c_b2ContactConstraintPoint::m_new(){
	return this;
}
void c_b2ContactConstraintPoint::mark(){
	Object::mark();
	gc_mark_q(m_localPoint);
	gc_mark_q(m_rA);
	gc_mark_q(m_rB);
}
c_b2WorldManifold::c_b2WorldManifold(){
	m_m_points=Array<c_b2Vec2* >();
	m_m_normal=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
}
c_b2WorldManifold* c_b2WorldManifold::m_new(){
	gc_assign(m_m_points,Array<c_b2Vec2* >(2));
	for(int t_i=0;t_i<2;t_i=t_i+1){
		gc_assign(m_m_points[t_i],(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0)));
	}
	return this;
}
void c_b2WorldManifold::p_Initialize3(c_b2Manifold* t_manifold,c_b2Transform* t_xfA,Float t_radiusA,c_b2Transform* t_xfB,Float t_radiusB){
	if(t_manifold->m_m_pointCount==0){
		return;
	}
	int t_i=0;
	c_b2Vec2* t_tVec=0;
	c_b2Mat22* t_tMat=0;
	Float t_normalX=FLOAT(.0);
	Float t_normalY=FLOAT(.0);
	Float t_planePointX=FLOAT(.0);
	Float t_planePointY=FLOAT(.0);
	Float t_clipPointX=FLOAT(.0);
	Float t_clipPointY=FLOAT(.0);
	int t_1=t_manifold->m_m_type;
	if(t_1==1){
		t_tMat=t_xfA->m_R;
		t_tVec=t_manifold->m_m_localPoint;
		Float t_pointAX=t_xfA->m_position->m_x+t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y;
		Float t_pointAY=t_xfA->m_position->m_y+t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y;
		t_tMat=t_xfB->m_R;
		t_tVec=t_manifold->m_m_points[0]->m_m_localPoint;
		Float t_pointBX=t_xfB->m_position->m_x+t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y;
		Float t_pointBY=t_xfB->m_position->m_y+t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y;
		Float t_dX=t_pointBX-t_pointAX;
		Float t_dY=t_pointBY-t_pointAY;
		Float t_d2=t_dX*t_dX+t_dY*t_dY;
		if(t_d2>FLOAT(1.0000000000000001e-030)){
			Float t_d=(Float)sqrt(t_d2);
			m_m_normal->m_x=t_dX/t_d;
			m_m_normal->m_y=t_dY/t_d;
		}else{
			m_m_normal->m_x=FLOAT(1.0);
			m_m_normal->m_y=FLOAT(0.0);
		}
		Float t_cAX=t_pointAX+t_radiusA*m_m_normal->m_x;
		Float t_cAY=t_pointAY+t_radiusA*m_m_normal->m_y;
		Float t_cBX=t_pointBX-t_radiusB*m_m_normal->m_x;
		Float t_cBY=t_pointBY-t_radiusB*m_m_normal->m_y;
		m_m_points[0]->m_x=FLOAT(0.5)*(t_cAX+t_cBX);
		m_m_points[0]->m_y=FLOAT(0.5)*(t_cAY+t_cBY);
	}else{
		if(t_1==2){
			t_tMat=t_xfA->m_R;
			t_tVec=t_manifold->m_m_localPlaneNormal;
			t_normalX=t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y;
			t_normalY=t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y;
			t_tMat=t_xfA->m_R;
			t_tVec=t_manifold->m_m_localPoint;
			t_planePointX=t_xfA->m_position->m_x+t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y;
			t_planePointY=t_xfA->m_position->m_y+t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y;
			m_m_normal->m_x=t_normalX;
			m_m_normal->m_y=t_normalY;
			for(int t_i2=0;t_i2<t_manifold->m_m_pointCount;t_i2=t_i2+1){
				t_tMat=t_xfB->m_R;
				t_tVec=t_manifold->m_m_points[t_i2]->m_m_localPoint;
				t_clipPointX=t_xfB->m_position->m_x+t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y;
				t_clipPointY=t_xfB->m_position->m_y+t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y;
				m_m_points[t_i2]->m_x=t_clipPointX+FLOAT(0.5)*(t_radiusA-(t_clipPointX-t_planePointX)*t_normalX-(t_clipPointY-t_planePointY)*t_normalY-t_radiusB)*t_normalX;
				m_m_points[t_i2]->m_y=t_clipPointY+FLOAT(0.5)*(t_radiusA-(t_clipPointX-t_planePointX)*t_normalX-(t_clipPointY-t_planePointY)*t_normalY-t_radiusB)*t_normalY;
			}
		}else{
			if(t_1==4){
				t_tMat=t_xfB->m_R;
				t_tVec=t_manifold->m_m_localPlaneNormal;
				t_normalX=t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y;
				t_normalY=t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y;
				t_tMat=t_xfB->m_R;
				t_tVec=t_manifold->m_m_localPoint;
				t_planePointX=t_xfB->m_position->m_x+t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y;
				t_planePointY=t_xfB->m_position->m_y+t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y;
				m_m_normal->m_x=-t_normalX;
				m_m_normal->m_y=-t_normalY;
				for(int t_i3=0;t_i3<t_manifold->m_m_pointCount;t_i3=t_i3+1){
					t_tMat=t_xfA->m_R;
					t_tVec=t_manifold->m_m_points[t_i3]->m_m_localPoint;
					t_clipPointX=t_xfA->m_position->m_x+t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y;
					t_clipPointY=t_xfA->m_position->m_y+t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y;
					m_m_points[t_i3]->m_x=t_clipPointX+FLOAT(0.5)*(t_radiusB-(t_clipPointX-t_planePointX)*t_normalX-(t_clipPointY-t_planePointY)*t_normalY-t_radiusA)*t_normalX;
					m_m_points[t_i3]->m_y=t_clipPointY+FLOAT(0.5)*(t_radiusB-(t_clipPointX-t_planePointX)*t_normalX-(t_clipPointY-t_planePointY)*t_normalY-t_radiusA)*t_normalY;
				}
			}
		}
	}
}
void c_b2WorldManifold::mark(){
	Object::mark();
	gc_mark_q(m_m_points);
	gc_mark_q(m_m_normal);
}
c_b2PositionSolverManifold::c_b2PositionSolverManifold(){
	m_m_normal=0;
	m_m_separations=Array<Float >();
	m_m_points=Array<c_b2Vec2* >();
}
c_b2PositionSolverManifold* c_b2PositionSolverManifold::m_new(){
	gc_assign(m_m_normal,(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0)));
	gc_assign(m_m_separations,Array<Float >(2));
	gc_assign(m_m_points,Array<c_b2Vec2* >(2));
	for(int t_i=0;t_i<m_m_points.Length();t_i=t_i+1){
		gc_assign(m_m_points[t_i],(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0)));
	}
	return this;
}
void c_b2PositionSolverManifold::p_Initialize4(c_b2ContactConstraint* t_cc){
	int t_i=0;
	int t_pointCount=t_cc->m_pointCount;
	Float t_clipPointX=FLOAT(.0);
	Float t_clipPointY=FLOAT(.0);
	c_b2Transform* t_tTrans=0;
	c_b2Mat22* t_tMat=0;
	c_b2Vec2* t_tVec=0;
	c_b2Vec2* t_tmpPos=0;
	Float t_planePointX=FLOAT(.0);
	Float t_planePointY=FLOAT(.0);
	int t_1=t_cc->m_type;
	if(t_1==1){
		t_tTrans=t_cc->m_bodyA->m_m_xf;
		t_tMat=t_tTrans->m_R;
		t_tVec=t_cc->m_localPoint;
		t_tmpPos=t_tTrans->m_position;
		Float t_pointAX=t_tmpPos->m_x+(t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y);
		Float t_pointAY=t_tmpPos->m_y+(t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y);
		t_tTrans=t_cc->m_bodyB->m_m_xf;
		t_tMat=t_tTrans->m_R;
		t_tVec=t_cc->m_points[0]->m_localPoint;
		t_tmpPos=t_tTrans->m_position;
		Float t_pointBX=t_tmpPos->m_x+(t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y);
		Float t_pointBY=t_tmpPos->m_y+(t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y);
		Float t_dX=t_pointBX-t_pointAX;
		Float t_dY=t_pointBY-t_pointAY;
		Float t_d2=t_dX*t_dX+t_dY*t_dY;
		if(t_d2>FLOAT(1.0000000000000001e-030)){
			Float t_d=(Float)sqrt(t_d2);
			m_m_normal->m_x=t_dX/t_d;
			m_m_normal->m_y=t_dY/t_d;
		}else{
			m_m_normal->m_x=FLOAT(1.0);
			m_m_normal->m_y=FLOAT(0.0);
		}
		m_m_points[0]->m_x=FLOAT(0.5)*(t_pointAX+t_pointBX);
		m_m_points[0]->m_y=FLOAT(0.5)*(t_pointAY+t_pointBY);
		m_m_separations[0]=t_dX*m_m_normal->m_x+t_dY*m_m_normal->m_y-t_cc->m_radius;
	}else{
		if(t_1==2){
			t_tTrans=t_cc->m_bodyA->m_m_xf;
			t_tMat=t_tTrans->m_R;
			t_tVec=t_cc->m_localPlaneNormal;
			t_tmpPos=t_tTrans->m_position;
			c_b2Vec2* t_tMatCol1=t_tMat->m_col1;
			c_b2Vec2* t_tMatCol2=t_tMat->m_col2;
			Float t_mc1X=t_tMatCol1->m_x;
			Float t_mc1Y=t_tMatCol1->m_y;
			Float t_mc2X=t_tMatCol2->m_x;
			Float t_mc2Y=t_tMatCol2->m_y;
			m_m_normal->m_x=t_mc1X*t_tVec->m_x+t_mc2X*t_tVec->m_y;
			m_m_normal->m_y=t_mc1Y*t_tVec->m_x+t_mc2Y*t_tVec->m_y;
			t_tVec=t_cc->m_localPoint;
			t_planePointX=t_tmpPos->m_x+(t_mc1X*t_tVec->m_x+t_mc2X*t_tVec->m_y);
			t_planePointY=t_tmpPos->m_y+(t_mc1Y*t_tVec->m_x+t_mc2Y*t_tVec->m_y);
			t_tTrans=t_cc->m_bodyB->m_m_xf;
			t_tMat=t_tTrans->m_R;
			t_tmpPos=t_tTrans->m_position;
			t_tMatCol1=t_tMat->m_col1;
			t_tMatCol2=t_tMat->m_col2;
			Float t_normX=m_m_normal->m_x;
			Float t_normY=m_m_normal->m_y;
			Float t_ccRad=t_cc->m_radius;
			t_mc1X=t_tMatCol1->m_x;
			t_mc1Y=t_tMatCol1->m_y;
			t_mc2X=t_tMatCol2->m_x;
			t_mc2Y=t_tMatCol2->m_y;
			Float t_tmpX=t_tmpPos->m_x;
			Float t_tmpY=t_tmpPos->m_y;
			for(int t_i2=0;t_i2<t_pointCount;t_i2=t_i2+1){
				t_tVec=t_cc->m_points[t_i2]->m_localPoint;
				t_clipPointX=t_tmpX+(t_mc1X*t_tVec->m_x+t_mc2X*t_tVec->m_y);
				t_clipPointY=t_tmpY+(t_mc1Y*t_tVec->m_x+t_mc2Y*t_tVec->m_y);
				m_m_separations[t_i2]=(t_clipPointX-t_planePointX)*t_normX+(t_clipPointY-t_planePointY)*t_normY-t_ccRad;
				m_m_points[t_i2]->m_x=t_clipPointX;
				m_m_points[t_i2]->m_y=t_clipPointY;
			}
		}else{
			if(t_1==4){
				t_tTrans=t_cc->m_bodyB->m_m_xf;
				t_tMat=t_tTrans->m_R;
				t_tmpPos=t_tTrans->m_position;
				t_tVec=t_cc->m_localPlaneNormal;
				c_b2Vec2* t_tMatCol12=t_tMat->m_col1;
				c_b2Vec2* t_tMatCol22=t_tMat->m_col2;
				Float t_mc1X2=t_tMatCol12->m_x;
				Float t_mc1Y2=t_tMatCol12->m_y;
				Float t_mc2X2=t_tMatCol22->m_x;
				Float t_mc2Y2=t_tMatCol22->m_y;
				m_m_normal->m_x=t_mc1X2*t_tVec->m_x+t_mc2X2*t_tVec->m_y;
				m_m_normal->m_y=t_mc1Y2*t_tVec->m_x+t_mc2Y2*t_tVec->m_y;
				t_tVec=t_cc->m_localPoint;
				t_planePointX=t_tmpPos->m_x+(t_mc1X2*t_tVec->m_x+t_mc2X2*t_tVec->m_y);
				t_planePointY=t_tmpPos->m_y+(t_mc1Y2*t_tVec->m_x+t_mc2Y2*t_tVec->m_y);
				t_tTrans=t_cc->m_bodyA->m_m_xf;
				t_tMat=t_tTrans->m_R;
				t_tmpPos=t_tTrans->m_position;
				t_tMatCol12=t_tMat->m_col1;
				t_tMatCol22=t_tMat->m_col2;
				Float t_normX2=m_m_normal->m_x;
				Float t_normY2=m_m_normal->m_y;
				Float t_ccRad2=t_cc->m_radius;
				t_mc1X2=t_tMatCol12->m_x;
				t_mc1Y2=t_tMatCol12->m_y;
				t_mc2X2=t_tMatCol22->m_x;
				t_mc2Y2=t_tMatCol22->m_y;
				Float t_tmpX2=t_tmpPos->m_x;
				Float t_tmpY2=t_tmpPos->m_y;
				for(int t_i3=0;t_i3<t_pointCount;t_i3=t_i3+1){
					t_tVec=t_cc->m_points[t_i3]->m_localPoint;
					t_clipPointX=t_tmpX2+(t_mc1X2*t_tVec->m_x+t_mc2X2*t_tVec->m_y);
					t_clipPointY=t_tmpY2+(t_mc1Y2*t_tVec->m_x+t_mc2Y2*t_tVec->m_y);
					m_m_separations[t_i3]=(t_clipPointX-t_planePointX)*t_normX2+(t_clipPointY-t_planePointY)*t_normY2-t_ccRad2;
					m_m_points[t_i3]->m_x=t_clipPointX;
					m_m_points[t_i3]->m_y=t_clipPointY;
				}
				m_m_normal->m_x=m_m_normal->m_x*FLOAT(-1.0);
				m_m_normal->m_y=m_m_normal->m_y*FLOAT(-1.0);
			}
		}
	}
}
void c_b2PositionSolverManifold::mark(){
	Object::mark();
	gc_mark_q(m_m_normal);
	gc_mark_q(m_m_separations);
	gc_mark_q(m_m_points);
}
c_b2ContactImpulse::c_b2ContactImpulse(){
	m_normalImpulses=Array<Float >(2);
	m_tangentImpulses=Array<Float >(2);
}
c_b2ContactImpulse* c_b2ContactImpulse::m_new(){
	return this;
}
void c_b2ContactImpulse::mark(){
	Object::mark();
	gc_mark_q(m_normalImpulses);
	gc_mark_q(m_tangentImpulses);
}
c_b2TOIInput::c_b2TOIInput(){
	m_proxyA=(new c_b2DistanceProxy)->m_new();
	m_proxyB=(new c_b2DistanceProxy)->m_new();
	m_sweepA=(new c_b2Sweep)->m_new();
	m_sweepB=(new c_b2Sweep)->m_new();
	m_tolerance=FLOAT(.0);
}
c_b2TOIInput* c_b2TOIInput::m_new(){
	return this;
}
void c_b2TOIInput::mark(){
	Object::mark();
	gc_mark_q(m_proxyA);
	gc_mark_q(m_proxyB);
	gc_mark_q(m_sweepA);
	gc_mark_q(m_sweepB);
}
c_b2TimeOfImpact::c_b2TimeOfImpact(){
}
int c_b2TimeOfImpact::m_b2_toiCalls;
c_b2SimplexCache* c_b2TimeOfImpact::m_s_cache;
c_b2DistanceInput* c_b2TimeOfImpact::m_s_distanceInput;
c_b2Transform* c_b2TimeOfImpact::m_s_xfA;
c_b2Transform* c_b2TimeOfImpact::m_s_xfB;
c_b2DistanceOutput* c_b2TimeOfImpact::m_s_distanceOutput;
c_b2SeparationFunction* c_b2TimeOfImpact::m_s_fcn;
int c_b2TimeOfImpact::m_b2_toiRootIters;
int c_b2TimeOfImpact::m_b2_toiMaxRootIters;
int c_b2TimeOfImpact::m_b2_toiIters;
int c_b2TimeOfImpact::m_b2_toiMaxIters;
Float c_b2TimeOfImpact::m_TimeOfImpact(c_b2TOIInput* t_input){
	m_b2_toiCalls+=1;
	c_b2DistanceProxy* t_proxyA=t_input->m_proxyA;
	c_b2DistanceProxy* t_proxyB=t_input->m_proxyB;
	c_b2Sweep* t_sweepA=t_input->m_sweepA;
	c_b2Sweep* t_sweepB=t_input->m_sweepB;
	Float t_radius=t_proxyA->m_m_radius+t_proxyB->m_m_radius;
	Float t_tolerance=t_input->m_tolerance;
	Float t_alpha=FLOAT(0.0);
	int t_iter=0;
	Float t_target=FLOAT(0.0);
	m_s_cache->m_count=0;
	m_s_distanceInput->m_useRadii=false;
	while(true){
		t_sweepA->p_GetTransform2(m_s_xfA,t_alpha);
		t_sweepB->p_GetTransform2(m_s_xfB,t_alpha);
		gc_assign(m_s_distanceInput->m_proxyA,t_proxyA);
		gc_assign(m_s_distanceInput->m_proxyB,t_proxyB);
		gc_assign(m_s_distanceInput->m_transformA,m_s_xfA);
		gc_assign(m_s_distanceInput->m_transformB,m_s_xfB);
		c_b2Distance::m_Distance(m_s_distanceOutput,m_s_cache,m_s_distanceInput);
		if(m_s_distanceOutput->m_distance<=FLOAT(0.0)){
			t_alpha=FLOAT(1.0);
			break;
		}
		m_s_fcn->p_Initialize5(m_s_cache,t_proxyA,t_sweepA,t_proxyB,t_sweepB,t_alpha);
		Float t_separation=m_s_fcn->p_Evaluate2(m_s_xfA,m_s_xfB);
		if(t_separation<=FLOAT(0.0)){
			t_alpha=FLOAT(1.0);
			break;
		}
		if(t_iter==0){
			if(t_separation>t_radius){
				t_target=c_b2Math::m_Max(t_radius-t_tolerance,FLOAT(0.75)*t_radius);
			}else{
				t_target=c_b2Math::m_Max(t_separation-t_tolerance,FLOAT(0.02)*t_radius);
			}
		}
		if(t_separation-t_target<FLOAT(0.5)*t_tolerance){
			if(t_iter==0){
				t_alpha=FLOAT(1.0);
				break;
			}
			break;
		}
		Float t_newAlpha=t_alpha;
		Float t_x1=t_alpha;
		Float t_x2=FLOAT(1.0);
		Float t_f1=t_separation;
		t_sweepA->p_GetTransform2(m_s_xfA,t_x2);
		t_sweepB->p_GetTransform2(m_s_xfB,t_x2);
		Float t_f2=m_s_fcn->p_Evaluate2(m_s_xfA,m_s_xfB);
		if(t_f2>=t_target){
			t_alpha=FLOAT(1.0);
			break;
		}
		int t_rootIterCount=0;
		while(true){
			Float t_x=FLOAT(.0);
			if((t_rootIterCount&1)!=0){
				t_x=t_x1+(t_target-t_f1)*(t_x2-t_x1)/(t_f2-t_f1);
			}else{
				t_x=FLOAT(0.5)*(t_x1+t_x2);
			}
			t_sweepA->p_GetTransform2(m_s_xfA,t_x);
			t_sweepB->p_GetTransform2(m_s_xfB,t_x);
			Float t_f=m_s_fcn->p_Evaluate2(m_s_xfA,m_s_xfB);
			if(c_b2Math::m_Abs(t_f-t_target)<FLOAT(0.025)*t_tolerance){
				t_newAlpha=t_x;
				break;
			}
			if(t_f>t_target){
				t_x1=t_x;
				t_f1=t_f;
			}else{
				t_x2=t_x;
				t_f2=t_f;
			}
			t_rootIterCount+=1;
			m_b2_toiRootIters+=1;
			if(t_rootIterCount==50){
				break;
			}
		}
		m_b2_toiMaxRootIters=int(c_b2Math::m_Max(Float(m_b2_toiMaxRootIters),Float(t_rootIterCount)));
		if(t_newAlpha<FLOAT(1.0000000000000999)*t_alpha){
			break;
		}
		t_alpha=t_newAlpha;
		t_iter+=1;
		m_b2_toiIters+=1;
		if(t_iter==1000){
			break;
		}
	}
	m_b2_toiMaxIters=int(c_b2Math::m_Max(Float(m_b2_toiMaxIters),Float(t_iter)));
	return t_alpha;
}
void c_b2TimeOfImpact::mark(){
	Object::mark();
}
c_b2SeparationFunction::c_b2SeparationFunction(){
	m_m_proxyA=0;
	m_m_proxyB=0;
	m_m_sweepA=0;
	m_m_sweepB=0;
	m_m_type=0;
	m_m_axis=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_m_localPoint=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
}
c_b2SeparationFunction* c_b2SeparationFunction::m_new(){
	return this;
}
c_b2Transform* c_b2SeparationFunction::m_tmpTransA;
c_b2Transform* c_b2SeparationFunction::m_tmpTransB;
c_b2Vec2* c_b2SeparationFunction::m_tmpVec1;
c_b2Vec2* c_b2SeparationFunction::m_tmpVec2;
c_b2Vec2* c_b2SeparationFunction::m_tmpVec3;
void c_b2SeparationFunction::p_Initialize5(c_b2SimplexCache* t_cache,c_b2DistanceProxy* t_proxyA,c_b2Sweep* t_sweepA,c_b2DistanceProxy* t_proxyB,c_b2Sweep* t_sweepB,Float t_alpha){
	gc_assign(m_m_proxyA,t_proxyA);
	gc_assign(m_m_proxyB,t_proxyB);
	int t_count=t_cache->m_count;
	gc_assign(m_m_sweepA,t_sweepA);
	gc_assign(m_m_sweepB,t_sweepB);
	c_b2Transform* t_xfA=m_tmpTransA;
	c_b2Transform* t_xfB=m_tmpTransB;
	m_m_sweepA->p_GetTransform2(t_xfA,t_alpha);
	m_m_sweepB->p_GetTransform2(t_xfB,t_alpha);
	if(t_count==1){
		m_m_type=1;
		c_b2Vec2* t_localPointA=m_m_proxyA->p_GetVertex(t_cache->m_indexA[0]);
		c_b2Vec2* t_localPointB=m_m_proxyB->p_GetVertex(t_cache->m_indexB[0]);
		c_b2Math::m_MulX(t_xfA,t_localPointA,m_tmpVec1);
		c_b2Math::m_MulX(t_xfB,t_localPointB,m_tmpVec2);
		c_b2Math::m_SubtractVV(m_tmpVec2,m_tmpVec1,m_m_axis);
		m_m_axis->p_Normalize();
	}else{
		if(t_cache->m_indexA[0]==t_cache->m_indexA[1]){
			m_m_type=4;
			c_b2Vec2* t_localPointB1=t_proxyB->p_GetVertex(t_cache->m_indexB[0]);
			c_b2Vec2* t_localPointB2=t_proxyB->p_GetVertex(t_cache->m_indexB[1]);
			c_b2Math::m_SubtractVV(t_localPointB2,t_localPointB1,m_m_axis);
			c_b2Math::m_CrossVF(m_m_axis,FLOAT(1.0),m_m_axis);
			m_m_axis->p_Normalize();
			c_b2Vec2* t_normal=m_tmpVec1;
			c_b2Math::m_MulMV(t_xfB->m_R,m_m_axis,t_normal);
			c_b2Math::m_AddVV(t_localPointB1,t_localPointB2,m_m_localPoint);
			m_m_localPoint->p_Multiply(FLOAT(0.5));
			c_b2Math::m_MulX(t_xfB,m_m_localPoint,m_tmpVec2);
			c_b2Vec2* t_localPointA2=t_proxyA->p_GetVertex(t_cache->m_indexA[0]);
			c_b2Math::m_MulX(t_xfA,t_localPointA2,m_tmpVec3);
			c_b2Math::m_SubtractVV(m_tmpVec3,m_tmpVec2,m_tmpVec3);
			Float t_s=c_b2Math::m_Dot(m_tmpVec3,t_normal);
			if(t_s<FLOAT(0.0)){
				m_m_axis->p_NegativeSelf();
				t_s=-t_s;
			}
		}else{
			m_m_type=2;
			c_b2Vec2* t_localPointA1=m_m_proxyA->p_GetVertex(t_cache->m_indexA[0]);
			c_b2Vec2* t_localPointA22=m_m_proxyA->p_GetVertex(t_cache->m_indexA[1]);
			c_b2Math::m_SubtractVV(t_localPointA22,t_localPointA1,m_m_axis);
			c_b2Math::m_CrossVF(m_m_axis,FLOAT(1.0),m_m_axis);
			m_m_axis->p_Normalize();
			c_b2Vec2* t_normal2=m_tmpVec3;
			c_b2Math::m_MulMV(t_xfA->m_R,m_m_axis,t_normal2);
			c_b2Math::m_AddVV(t_localPointA1,t_localPointA22,m_m_localPoint);
			m_m_localPoint->p_Multiply(FLOAT(0.5));
			c_b2Math::m_MulX(t_xfA,m_m_localPoint,m_tmpVec1);
			c_b2Vec2* t_localPointB3=m_m_proxyB->p_GetVertex(t_cache->m_indexB[0]);
			c_b2Math::m_MulX(t_xfB,t_localPointB3,m_tmpVec2);
			c_b2Math::m_SubtractVV(m_tmpVec2,m_tmpVec1,m_tmpVec1);
			Float t_s2=c_b2Math::m_Dot(m_tmpVec1,t_normal2);
			if(t_s2<FLOAT(0.0)){
				m_m_axis->p_NegativeSelf();
				t_s2=-t_s2;
			}
		}
	}
}
Float c_b2SeparationFunction::p_Evaluate2(c_b2Transform* t_transformA,c_b2Transform* t_transformB){
	c_b2Vec2* t_axisA=0;
	c_b2Vec2* t_axisB=0;
	c_b2Vec2* t_localPointA=0;
	c_b2Vec2* t_localPointB=0;
	c_b2Vec2* t_pointA=0;
	c_b2Vec2* t_pointB=0;
	Float t_separation=FLOAT(.0);
	c_b2Vec2* t_normal=0;
	int t_1=m_m_type;
	if(t_1==1){
		t_axisA=m_tmpVec1;
		t_axisB=m_tmpVec2;
		c_b2Math::m_MulTMV(t_transformA->m_R,m_m_axis,t_axisA);
		m_m_axis->p_GetNegative(t_axisB);
		c_b2Math::m_MulTMV(t_transformB->m_R,t_axisB,t_axisB);
		t_localPointA=m_m_proxyA->p_GetSupportVertex(t_axisA);
		t_localPointB=m_m_proxyB->p_GetSupportVertex(t_axisB);
		t_pointA=m_tmpVec1;
		t_pointB=m_tmpVec2;
		c_b2Math::m_MulX(t_transformA,t_localPointA,t_pointA);
		c_b2Math::m_MulX(t_transformB,t_localPointB,t_pointB);
		t_separation=(t_pointB->m_x-t_pointA->m_x)*m_m_axis->m_x+(t_pointB->m_y-t_pointA->m_y)*m_m_axis->m_y;
		return t_separation;
	}else{
		if(t_1==2){
			t_normal=m_tmpVec1;
			t_pointA=m_tmpVec2;
			t_axisB=m_tmpVec3;
			c_b2Math::m_MulMV(t_transformA->m_R,m_m_axis,t_normal);
			c_b2Math::m_MulX(t_transformA,m_m_localPoint,t_pointA);
			t_normal->p_GetNegative(t_axisB);
			c_b2Math::m_MulTMV(t_transformB->m_R,t_axisB,t_axisB);
			t_localPointB=m_m_proxyB->p_GetSupportVertex(t_axisB);
			t_pointB=m_tmpVec3;
			c_b2Math::m_MulX(t_transformB,t_localPointB,t_pointB);
			t_separation=(t_pointB->m_x-t_pointA->m_x)*t_normal->m_x+(t_pointB->m_y-t_pointA->m_y)*t_normal->m_y;
			return t_separation;
		}else{
			if(t_1==4){
				t_normal=m_tmpVec1;
				t_pointB=m_tmpVec2;
				t_axisA=m_tmpVec3;
				c_b2Math::m_MulMV(t_transformB->m_R,m_m_axis,t_normal);
				c_b2Math::m_MulX(t_transformB,m_m_localPoint,t_pointB);
				t_normal->p_GetNegative(t_axisA);
				c_b2Math::m_MulTMV(t_transformA->m_R,t_axisA,t_axisA);
				t_localPointA=m_m_proxyA->p_GetSupportVertex(t_axisA);
				t_pointA=m_tmpVec3;
				c_b2Math::m_MulX(t_transformA,t_localPointA,t_pointA);
				t_separation=(t_pointA->m_x-t_pointB->m_x)*t_normal->m_x+(t_pointA->m_y-t_pointB->m_y)*t_normal->m_y;
				return t_separation;
			}else{
				c_b2Settings::m_B2Assert(false);
				return FLOAT(0.0);
			}
		}
	}
}
void c_b2SeparationFunction::mark(){
	Object::mark();
	gc_mark_q(m_m_proxyA);
	gc_mark_q(m_m_proxyB);
	gc_mark_q(m_m_sweepA);
	gc_mark_q(m_m_sweepB);
	gc_mark_q(m_m_axis);
	gc_mark_q(m_m_localPoint);
}
c_DoTweet::c_DoTweet(){
}
void c_DoTweet::m_LaunchTwitter(String t_twitter_name,String t_twitter_text,String t_hashtags){
	bb_Rebound_LaunchBrowser(String(L"http://twitter.com/share?screen_name=",37)+t_twitter_name+String(L"&text=",6)+t_twitter_text+String(L"&url=",5)+String()+String(L"&hashtags=",10)+t_hashtags,String(L"_blank",6));
}
void c_DoTweet::mark(){
	Object::mark();
}
void bb_Rebound_LaunchBrowser(String t_address,String t_windowName){
	browser::launchBrowser(t_address,t_windowName);
}
void bb_app_SetDeviceWindow(int t_width,int t_height,int t_flags){
	bb_app__game->SetDeviceWindow(t_width,t_height,t_flags);
	bb_app_ValidateDeviceWindow(false);
}
void bb_app_HideMouse(){
	bb_app__game->SetMouseVisible(false);
}
int bb_math_Max(int t_x,int t_y){
	if(t_x>t_y){
		return t_x;
	}
	return t_y;
}
Float bb_math_Max2(Float t_x,Float t_y){
	if(t_x>t_y){
		return t_x;
	}
	return t_y;
}
int bb_math_Min(int t_x,int t_y){
	if(t_x<t_y){
		return t_x;
	}
	return t_y;
}
Float bb_math_Min2(Float t_x,Float t_y){
	if(t_x<t_y){
		return t_x;
	}
	return t_y;
}
int bb_graphics_Cls(Float t_r,Float t_g,Float t_b){
	bb_graphics_renderDevice->Cls(t_r,t_g,t_b);
	return 0;
}
int bb_graphics_Transform(Float t_ix,Float t_iy,Float t_jx,Float t_jy,Float t_tx,Float t_ty){
	Float t_ix2=t_ix*bb_graphics_context->m_ix+t_iy*bb_graphics_context->m_jx;
	Float t_iy2=t_ix*bb_graphics_context->m_iy+t_iy*bb_graphics_context->m_jy;
	Float t_jx2=t_jx*bb_graphics_context->m_ix+t_jy*bb_graphics_context->m_jx;
	Float t_jy2=t_jx*bb_graphics_context->m_iy+t_jy*bb_graphics_context->m_jy;
	Float t_tx2=t_tx*bb_graphics_context->m_ix+t_ty*bb_graphics_context->m_jx+bb_graphics_context->m_tx;
	Float t_ty2=t_tx*bb_graphics_context->m_iy+t_ty*bb_graphics_context->m_jy+bb_graphics_context->m_ty;
	bb_graphics_SetMatrix(t_ix2,t_iy2,t_jx2,t_jy2,t_tx2,t_ty2);
	return 0;
}
int bb_graphics_Transform2(Array<Float > t_m){
	bb_graphics_Transform(t_m[0],t_m[1],t_m[2],t_m[3],t_m[4],t_m[5]);
	return 0;
}
int bb_graphics_Scale(Float t_x,Float t_y){
	bb_graphics_Transform(t_x,FLOAT(0.0),FLOAT(0.0),t_y,FLOAT(0.0),FLOAT(0.0));
	return 0;
}
int bb_graphics_Translate(Float t_x,Float t_y){
	bb_graphics_Transform(FLOAT(1.0),FLOAT(0.0),FLOAT(0.0),FLOAT(1.0),t_x,t_y);
	return 0;
}
int bb_autofit_UpdateVirtualDisplay(bool t_zoomborders,bool t_keepborders){
	c_VirtualDisplay::m_Display->p_UpdateVirtualDisplay(t_zoomborders,t_keepborders);
	return 0;
}
int bb_graphics_DrawImage(c_Image* t_image,Float t_x,Float t_y,int t_frame){
	c_Frame* t_f=t_image->m_frames[t_frame];
	bb_graphics_context->p_Validate();
	if((t_image->m_flags&65536)!=0){
		bb_graphics_renderDevice->DrawSurface(t_image->m_surface,t_x-t_image->m_tx,t_y-t_image->m_ty);
	}else{
		bb_graphics_renderDevice->DrawSurface2(t_image->m_surface,t_x-t_image->m_tx,t_y-t_image->m_ty,t_f->m_x,t_f->m_y,t_image->m_width,t_image->m_height);
	}
	return 0;
}
int bb_graphics_PushMatrix(){
	int t_sp=bb_graphics_context->m_matrixSp;
	if(t_sp==bb_graphics_context->m_matrixStack.Length()){
		gc_assign(bb_graphics_context->m_matrixStack,bb_graphics_context->m_matrixStack.Resize(t_sp*2));
	}
	bb_graphics_context->m_matrixStack[t_sp+0]=bb_graphics_context->m_ix;
	bb_graphics_context->m_matrixStack[t_sp+1]=bb_graphics_context->m_iy;
	bb_graphics_context->m_matrixStack[t_sp+2]=bb_graphics_context->m_jx;
	bb_graphics_context->m_matrixStack[t_sp+3]=bb_graphics_context->m_jy;
	bb_graphics_context->m_matrixStack[t_sp+4]=bb_graphics_context->m_tx;
	bb_graphics_context->m_matrixStack[t_sp+5]=bb_graphics_context->m_ty;
	bb_graphics_context->m_matrixSp=t_sp+6;
	return 0;
}
int bb_graphics_Rotate(Float t_angle){
	bb_graphics_Transform((Float)cos((t_angle)*D2R),-(Float)sin((t_angle)*D2R),(Float)sin((t_angle)*D2R),(Float)cos((t_angle)*D2R),FLOAT(0.0),FLOAT(0.0));
	return 0;
}
int bb_graphics_PopMatrix(){
	int t_sp=bb_graphics_context->m_matrixSp-6;
	bb_graphics_SetMatrix(bb_graphics_context->m_matrixStack[t_sp+0],bb_graphics_context->m_matrixStack[t_sp+1],bb_graphics_context->m_matrixStack[t_sp+2],bb_graphics_context->m_matrixStack[t_sp+3],bb_graphics_context->m_matrixStack[t_sp+4],bb_graphics_context->m_matrixStack[t_sp+5]);
	bb_graphics_context->m_matrixSp=t_sp;
	return 0;
}
int bb_graphics_DrawImage2(c_Image* t_image,Float t_x,Float t_y,Float t_rotation,Float t_scaleX,Float t_scaleY,int t_frame){
	c_Frame* t_f=t_image->m_frames[t_frame];
	bb_graphics_PushMatrix();
	bb_graphics_Translate(t_x,t_y);
	bb_graphics_Rotate(t_rotation);
	bb_graphics_Scale(t_scaleX,t_scaleY);
	bb_graphics_Translate(-t_image->m_tx,-t_image->m_ty);
	bb_graphics_context->p_Validate();
	if((t_image->m_flags&65536)!=0){
		bb_graphics_renderDevice->DrawSurface(t_image->m_surface,FLOAT(0.0),FLOAT(0.0));
	}else{
		bb_graphics_renderDevice->DrawSurface2(t_image->m_surface,FLOAT(0.0),FLOAT(0.0),t_f->m_x,t_f->m_y,t_image->m_width,t_image->m_height);
	}
	bb_graphics_PopMatrix();
	return 0;
}
c_eDrawAlign::c_eDrawAlign(){
}
void c_eDrawAlign::mark(){
	Object::mark();
}
c_eDrawMode::c_eDrawMode(){
}
void c_eDrawMode::mark(){
	Object::mark();
}
int bb_math_Abs(int t_x){
	if(t_x>=0){
		return t_x;
	}
	return -t_x;
}
Float bb_math_Abs2(Float t_x){
	if(t_x>=FLOAT(0.0)){
		return t_x;
	}
	return -t_x;
}
int bb_graphics_DrawImageRect(c_Image* t_image,Float t_x,Float t_y,int t_srcX,int t_srcY,int t_srcWidth,int t_srcHeight,int t_frame){
	c_Frame* t_f=t_image->m_frames[t_frame];
	bb_graphics_context->p_Validate();
	bb_graphics_renderDevice->DrawSurface2(t_image->m_surface,-t_image->m_tx+t_x,-t_image->m_ty+t_y,t_srcX+t_f->m_x,t_srcY+t_f->m_y,t_srcWidth,t_srcHeight);
	return 0;
}
int bb_graphics_DrawImageRect2(c_Image* t_image,Float t_x,Float t_y,int t_srcX,int t_srcY,int t_srcWidth,int t_srcHeight,Float t_rotation,Float t_scaleX,Float t_scaleY,int t_frame){
	c_Frame* t_f=t_image->m_frames[t_frame];
	bb_graphics_PushMatrix();
	bb_graphics_Translate(t_x,t_y);
	bb_graphics_Rotate(t_rotation);
	bb_graphics_Scale(t_scaleX,t_scaleY);
	bb_graphics_Translate(-t_image->m_tx,-t_image->m_ty);
	bb_graphics_context->p_Validate();
	bb_graphics_renderDevice->DrawSurface2(t_image->m_surface,FLOAT(0.0),FLOAT(0.0),t_srcX+t_f->m_x,t_srcY+t_f->m_y,t_srcWidth,t_srcHeight);
	bb_graphics_PopMatrix();
	return 0;
}
c_b2Color::c_b2Color(){
	m__r=0;
	m__g=0;
	m__b=0;
}
c_b2Color* c_b2Color::m_new(Float t_rr,Float t_gg,Float t_bb){
	m__r=int(FLOAT(255.0)*c_b2Math::m_Clamp(t_rr,FLOAT(0.0),FLOAT(1.0)));
	m__g=int(FLOAT(255.0)*c_b2Math::m_Clamp(t_gg,FLOAT(0.0),FLOAT(1.0)));
	m__b=int(FLOAT(255.0)*c_b2Math::m_Clamp(t_bb,FLOAT(0.0),FLOAT(1.0)));
	return this;
}
c_b2Color* c_b2Color::m_new2(){
	return this;
}
void c_b2Color::p_Set11(Float t_rr,Float t_gg,Float t_bb){
	m__r=int(FLOAT(255.0)*c_b2Math::m_Clamp(t_rr,FLOAT(0.0),FLOAT(1.0)));
	m__g=int(FLOAT(255.0)*c_b2Math::m_Clamp(t_gg,FLOAT(0.0),FLOAT(1.0)));
	m__b=int(FLOAT(255.0)*c_b2Math::m_Clamp(t_bb,FLOAT(0.0),FLOAT(1.0)));
}
void c_b2Color::mark(){
	Object::mark();
}
int bb_graphics_DrawCircle(Float t_x,Float t_y,Float t_r){
	bb_graphics_context->p_Validate();
	bb_graphics_renderDevice->DrawOval(t_x-t_r,t_y-t_r,t_r*FLOAT(2.0),t_r*FLOAT(2.0));
	return 0;
}
int bb_graphics_DrawLine(Float t_x1,Float t_y1,Float t_x2,Float t_y2){
	bb_graphics_context->p_Validate();
	bb_graphics_renderDevice->DrawLine(t_x1,t_y1,t_x2,t_y2);
	return 0;
}
c_b2EdgeShape::c_b2EdgeShape(){
	m_m_v1=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_m_v2=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_m_prevEdge=0;
	m_m_nextEdge=0;
	m_m_direction=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_m_length=FLOAT(.0);
	m_m_normal=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_m_coreV1=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_m_coreV2=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_m_cornerDir1=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_m_cornerDir2=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
}
c_b2Vec2* c_b2EdgeShape::p_GetVertex1(){
	return m_m_v1;
}
c_b2Vec2* c_b2EdgeShape::p_GetVertex2(){
	return m_m_v2;
}
void c_b2EdgeShape::p_ComputeAABB(c_b2AABB* t_aabb,c_b2Transform* t_transform){
	c_b2Mat22* t_tMat=t_transform->m_R;
	Float t_v1X=t_transform->m_position->m_x+(t_tMat->m_col1->m_x*m_m_v1->m_x+t_tMat->m_col2->m_x*m_m_v1->m_y);
	Float t_v1Y=t_transform->m_position->m_y+(t_tMat->m_col1->m_y*m_m_v1->m_x+t_tMat->m_col2->m_y*m_m_v1->m_y);
	Float t_v2X=t_transform->m_position->m_x+(t_tMat->m_col1->m_x*m_m_v2->m_x+t_tMat->m_col2->m_x*m_m_v2->m_y);
	Float t_v2Y=t_transform->m_position->m_y+(t_tMat->m_col1->m_y*m_m_v2->m_x+t_tMat->m_col2->m_y*m_m_v2->m_y);
	if(t_v1X<t_v2X){
		t_aabb->m_lowerBound->m_x=t_v1X;
		t_aabb->m_upperBound->m_x=t_v2X;
	}else{
		t_aabb->m_lowerBound->m_x=t_v2X;
		t_aabb->m_upperBound->m_x=t_v1X;
	}
	if(t_v1Y<t_v2Y){
		t_aabb->m_lowerBound->m_y=t_v1Y;
		t_aabb->m_upperBound->m_y=t_v2Y;
	}else{
		t_aabb->m_lowerBound->m_y=t_v2Y;
		t_aabb->m_upperBound->m_y=t_v1Y;
	}
}
void c_b2EdgeShape::p_ComputeMass(c_b2MassData* t_massData,Float t_density){
	t_massData->m_mass=FLOAT(0.0);
	t_massData->m_center->p_SetV(m_m_v1);
	t_massData->m_I=FLOAT(0.0);
}
c_b2EdgeShape* c_b2EdgeShape::m_new(c_b2Vec2* t_v1,c_b2Vec2* t_v2){
	c_b2Shape::m_new();
	m_m_type=2;
	m_m_prevEdge=0;
	m_m_nextEdge=0;
	gc_assign(m_m_v1,t_v1);
	gc_assign(m_m_v2,t_v2);
	m_m_direction->p_Set2(m_m_v2->m_x-m_m_v1->m_x,m_m_v2->m_y-m_m_v1->m_y);
	m_m_length=m_m_direction->p_Normalize();
	m_m_normal->p_Set2(m_m_direction->m_y,-m_m_direction->m_x);
	m_m_coreV1->p_Set2(FLOAT(-0.040000000000000001)*(m_m_normal->m_x-m_m_direction->m_x)+m_m_v1->m_x,FLOAT(-0.040000000000000001)*(m_m_normal->m_y-m_m_direction->m_y)+m_m_v1->m_y);
	m_m_coreV2->p_Set2(FLOAT(-0.040000000000000001)*(m_m_normal->m_x+m_m_direction->m_x)+m_m_v2->m_x,FLOAT(-0.040000000000000001)*(m_m_normal->m_y+m_m_direction->m_y)+m_m_v2->m_y);
	gc_assign(m_m_cornerDir1,m_m_normal);
	m_m_cornerDir2->p_Set2(-m_m_normal->m_x,-m_m_normal->m_y);
	return this;
}
c_b2EdgeShape* c_b2EdgeShape::m_new2(){
	c_b2Shape::m_new();
	return this;
}
c_b2Shape* c_b2EdgeShape::p_Copy(){
	c_b2EdgeShape* t_s=(new c_b2EdgeShape)->m_new(this->m_m_v1->p_Copy(),this->m_m_v2->p_Copy());
	return (t_s);
}
void c_b2EdgeShape::mark(){
	c_b2Shape::mark();
	gc_mark_q(m_m_v1);
	gc_mark_q(m_m_v2);
	gc_mark_q(m_m_prevEdge);
	gc_mark_q(m_m_nextEdge);
	gc_mark_q(m_m_direction);
	gc_mark_q(m_m_normal);
	gc_mark_q(m_m_coreV1);
	gc_mark_q(m_m_coreV2);
	gc_mark_q(m_m_cornerDir1);
	gc_mark_q(m_m_cornerDir2);
}
c_b2PulleyJoint::c_b2PulleyJoint(){
	m_m_ground=0;
	m_m_groundAnchor1=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_m_groundAnchor2=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_m_localAnchor1=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_m_localAnchor2=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_m_u1=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_m_u2=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_m_constant=FLOAT(.0);
	m_m_ratio=FLOAT(.0);
	m_m_state=0;
	m_m_impulse=FLOAT(.0);
	m_m_maxLength1=FLOAT(.0);
	m_m_limitState1=0;
	m_m_limitImpulse1=FLOAT(.0);
	m_m_maxLength2=FLOAT(.0);
	m_m_limitState2=0;
	m_m_limitImpulse2=FLOAT(.0);
	m_m_limitMass1=FLOAT(.0);
	m_m_limitMass2=FLOAT(.0);
	m_m_pulleyMass=FLOAT(.0);
}
void c_b2PulleyJoint::p_GetGroundAnchorA(c_b2Vec2* t_out){
	t_out->p_SetV(m_m_ground->m_m_xf->m_position);
	t_out->p_Add(m_m_groundAnchor1);
}
void c_b2PulleyJoint::p_GetGroundAnchorB(c_b2Vec2* t_out){
	t_out->p_SetV(m_m_ground->m_m_xf->m_position);
	t_out->p_Add(m_m_groundAnchor2);
}
void c_b2PulleyJoint::p_GetAnchorA(c_b2Vec2* t_out){
	m_m_bodyA->p_GetWorldPoint(m_m_localAnchor1,t_out);
}
void c_b2PulleyJoint::p_GetAnchorB(c_b2Vec2* t_out){
	m_m_bodyB->p_GetWorldPoint(m_m_localAnchor2,t_out);
}
void c_b2PulleyJoint::p_InitVelocityConstraints(c_b2TimeStep* t_timeStep){
	c_b2Body* t_bA=m_m_bodyA;
	c_b2Body* t_bB=m_m_bodyB;
	c_b2Mat22* t_tMat=0;
	t_tMat=t_bA->m_m_xf->m_R;
	Float t_r1X=m_m_localAnchor1->m_x-t_bA->m_m_sweep->m_localCenter->m_x;
	Float t_r1Y=m_m_localAnchor1->m_y-t_bA->m_m_sweep->m_localCenter->m_y;
	Float t_tX=t_tMat->m_col1->m_x*t_r1X+t_tMat->m_col2->m_x*t_r1Y;
	t_r1Y=t_tMat->m_col1->m_y*t_r1X+t_tMat->m_col2->m_y*t_r1Y;
	t_r1X=t_tX;
	t_tMat=t_bB->m_m_xf->m_R;
	Float t_r2X=m_m_localAnchor2->m_x-t_bB->m_m_sweep->m_localCenter->m_x;
	Float t_r2Y=m_m_localAnchor2->m_y-t_bB->m_m_sweep->m_localCenter->m_y;
	t_tX=t_tMat->m_col1->m_x*t_r2X+t_tMat->m_col2->m_x*t_r2Y;
	t_r2Y=t_tMat->m_col1->m_y*t_r2X+t_tMat->m_col2->m_y*t_r2Y;
	t_r2X=t_tX;
	Float t_p1X=t_bA->m_m_sweep->m_c->m_x+t_r1X;
	Float t_p1Y=t_bA->m_m_sweep->m_c->m_y+t_r1Y;
	Float t_p2X=t_bB->m_m_sweep->m_c->m_x+t_r2X;
	Float t_p2Y=t_bB->m_m_sweep->m_c->m_y+t_r2Y;
	Float t_s1X=m_m_ground->m_m_xf->m_position->m_x+m_m_groundAnchor1->m_x;
	Float t_s1Y=m_m_ground->m_m_xf->m_position->m_y+m_m_groundAnchor1->m_y;
	Float t_s2X=m_m_ground->m_m_xf->m_position->m_x+m_m_groundAnchor2->m_x;
	Float t_s2Y=m_m_ground->m_m_xf->m_position->m_y+m_m_groundAnchor2->m_y;
	m_m_u1->p_Set2(t_p1X-t_s1X,t_p1Y-t_s1Y);
	m_m_u2->p_Set2(t_p2X-t_s2X,t_p2Y-t_s2Y);
	Float t_length1=m_m_u1->p_Length();
	Float t_length2=m_m_u2->p_Length();
	if(t_length1>FLOAT(0.005)){
		m_m_u1->p_Multiply(FLOAT(1.0)/t_length1);
	}else{
		m_m_u1->p_SetZero();
	}
	if(t_length2>FLOAT(0.005)){
		m_m_u2->p_Multiply(FLOAT(1.0)/t_length2);
	}else{
		m_m_u2->p_SetZero();
	}
	Float t_C=m_m_constant-t_length1-m_m_ratio*t_length2;
	if(t_C>FLOAT(0.0)){
		m_m_state=0;
		m_m_impulse=FLOAT(0.0);
	}else{
		m_m_state=2;
	}
	if(t_length1<m_m_maxLength1){
		m_m_limitState1=0;
		m_m_limitImpulse1=FLOAT(0.0);
	}else{
		m_m_limitState1=2;
	}
	if(t_length2<m_m_maxLength2){
		m_m_limitState2=0;
		m_m_limitImpulse2=FLOAT(0.0);
	}else{
		m_m_limitState2=2;
	}
	Float t_cr1u1=t_r1X*m_m_u1->m_y-t_r1Y*m_m_u1->m_x;
	Float t_cr2u2=t_r2X*m_m_u2->m_y-t_r2Y*m_m_u2->m_x;
	m_m_limitMass1=t_bA->m_m_invMass+t_bA->m_m_invI*t_cr1u1*t_cr1u1;
	m_m_limitMass2=t_bB->m_m_invMass+t_bB->m_m_invI*t_cr2u2*t_cr2u2;
	m_m_pulleyMass=m_m_limitMass1+m_m_ratio*m_m_ratio*m_m_limitMass2;
	m_m_limitMass1=FLOAT(1.0)/m_m_limitMass1;
	m_m_limitMass2=FLOAT(1.0)/m_m_limitMass2;
	m_m_pulleyMass=FLOAT(1.0)/m_m_pulleyMass;
	if(t_timeStep->m_warmStarting){
		m_m_impulse*=t_timeStep->m_dtRatio;
		m_m_limitImpulse1*=t_timeStep->m_dtRatio;
		m_m_limitImpulse2*=t_timeStep->m_dtRatio;
		Float t_P1X2=(-m_m_impulse-m_m_limitImpulse1)*m_m_u1->m_x;
		Float t_P1Y2=(-m_m_impulse-m_m_limitImpulse1)*m_m_u1->m_y;
		Float t_P2X2=(-m_m_ratio*m_m_impulse-m_m_limitImpulse2)*m_m_u2->m_x;
		Float t_P2Y2=(-m_m_ratio*m_m_impulse-m_m_limitImpulse2)*m_m_u2->m_y;
		t_bA->m_m_linearVelocity->m_x+=t_bA->m_m_invMass*t_P1X2;
		t_bA->m_m_linearVelocity->m_y+=t_bA->m_m_invMass*t_P1Y2;
		t_bA->m_m_angularVelocity+=t_bA->m_m_invI*(t_r1X*t_P1Y2-t_r1Y*t_P1X2);
		t_bB->m_m_linearVelocity->m_x+=t_bB->m_m_invMass*t_P2X2;
		t_bB->m_m_linearVelocity->m_y+=t_bB->m_m_invMass*t_P2Y2;
		t_bB->m_m_angularVelocity+=t_bB->m_m_invI*(t_r2X*t_P2Y2-t_r2Y*t_P2X2);
	}else{
		m_m_impulse=FLOAT(0.0);
		m_m_limitImpulse1=FLOAT(0.0);
		m_m_limitImpulse2=FLOAT(0.0);
	}
}
void c_b2PulleyJoint::p_SolveVelocityConstraints(c_b2TimeStep* t_timeStep){
	c_b2Body* t_bA=m_m_bodyA;
	c_b2Body* t_bB=m_m_bodyB;
	c_b2Mat22* t_tMat=0;
	t_tMat=t_bA->m_m_xf->m_R;
	Float t_r1X=m_m_localAnchor1->m_x-t_bA->m_m_sweep->m_localCenter->m_x;
	Float t_r1Y=m_m_localAnchor1->m_y-t_bA->m_m_sweep->m_localCenter->m_y;
	Float t_tX=t_tMat->m_col1->m_x*t_r1X+t_tMat->m_col2->m_x*t_r1Y;
	t_r1Y=t_tMat->m_col1->m_y*t_r1X+t_tMat->m_col2->m_y*t_r1Y;
	t_r1X=t_tX;
	t_tMat=t_bB->m_m_xf->m_R;
	Float t_r2X=m_m_localAnchor2->m_x-t_bB->m_m_sweep->m_localCenter->m_x;
	Float t_r2Y=m_m_localAnchor2->m_y-t_bB->m_m_sweep->m_localCenter->m_y;
	t_tX=t_tMat->m_col1->m_x*t_r2X+t_tMat->m_col2->m_x*t_r2Y;
	t_r2Y=t_tMat->m_col1->m_y*t_r2X+t_tMat->m_col2->m_y*t_r2Y;
	t_r2X=t_tX;
	Float t_v1X=FLOAT(.0);
	Float t_v1Y=FLOAT(.0);
	Float t_v2X=FLOAT(.0);
	Float t_v2Y=FLOAT(.0);
	Float t_P1X=FLOAT(.0);
	Float t_P1Y=FLOAT(.0);
	Float t_P2X=FLOAT(.0);
	Float t_P2Y=FLOAT(.0);
	Float t_Cdot=FLOAT(.0);
	Float t_impulse=FLOAT(.0);
	Float t_oldImpulse=FLOAT(.0);
	if(m_m_state==2){
		t_v1X=t_bA->m_m_linearVelocity->m_x+-t_bA->m_m_angularVelocity*t_r1Y;
		t_v1Y=t_bA->m_m_linearVelocity->m_y+t_bA->m_m_angularVelocity*t_r1X;
		t_v2X=t_bB->m_m_linearVelocity->m_x+-t_bB->m_m_angularVelocity*t_r2Y;
		t_v2Y=t_bB->m_m_linearVelocity->m_y+t_bB->m_m_angularVelocity*t_r2X;
		t_Cdot=-(m_m_u1->m_x*t_v1X+m_m_u1->m_y*t_v1Y)-m_m_ratio*(m_m_u2->m_x*t_v2X+m_m_u2->m_y*t_v2Y);
		t_impulse=m_m_pulleyMass*-t_Cdot;
		t_oldImpulse=m_m_impulse;
		m_m_impulse=c_b2Math::m_Max(FLOAT(0.0),m_m_impulse+t_impulse);
		t_impulse=m_m_impulse-t_oldImpulse;
		t_P1X=-t_impulse*m_m_u1->m_x;
		t_P1Y=-t_impulse*m_m_u1->m_y;
		t_P2X=-m_m_ratio*t_impulse*m_m_u2->m_x;
		t_P2Y=-m_m_ratio*t_impulse*m_m_u2->m_y;
		t_bA->m_m_linearVelocity->m_x+=t_bA->m_m_invMass*t_P1X;
		t_bA->m_m_linearVelocity->m_y+=t_bA->m_m_invMass*t_P1Y;
		t_bA->m_m_angularVelocity+=t_bA->m_m_invI*(t_r1X*t_P1Y-t_r1Y*t_P1X);
		t_bB->m_m_linearVelocity->m_x+=t_bB->m_m_invMass*t_P2X;
		t_bB->m_m_linearVelocity->m_y+=t_bB->m_m_invMass*t_P2Y;
		t_bB->m_m_angularVelocity+=t_bB->m_m_invI*(t_r2X*t_P2Y-t_r2Y*t_P2X);
	}
	if(m_m_limitState1==2){
		t_v1X=t_bA->m_m_linearVelocity->m_x+-t_bA->m_m_angularVelocity*t_r1Y;
		t_v1Y=t_bA->m_m_linearVelocity->m_y+t_bA->m_m_angularVelocity*t_r1X;
		t_Cdot=-(m_m_u1->m_x*t_v1X+m_m_u1->m_y*t_v1Y);
		t_impulse=-m_m_limitMass1*t_Cdot;
		t_oldImpulse=m_m_limitImpulse1;
		m_m_limitImpulse1=c_b2Math::m_Max(FLOAT(0.0),m_m_limitImpulse1+t_impulse);
		t_impulse=m_m_limitImpulse1-t_oldImpulse;
		t_P1X=-t_impulse*m_m_u1->m_x;
		t_P1Y=-t_impulse*m_m_u1->m_y;
		t_bA->m_m_linearVelocity->m_x+=t_bA->m_m_invMass*t_P1X;
		t_bA->m_m_linearVelocity->m_y+=t_bA->m_m_invMass*t_P1Y;
		t_bA->m_m_angularVelocity+=t_bA->m_m_invI*(t_r1X*t_P1Y-t_r1Y*t_P1X);
	}
	if(m_m_limitState2==2){
		t_v2X=t_bB->m_m_linearVelocity->m_x+-t_bB->m_m_angularVelocity*t_r2Y;
		t_v2Y=t_bB->m_m_linearVelocity->m_y+t_bB->m_m_angularVelocity*t_r2X;
		t_Cdot=-(m_m_u2->m_x*t_v2X+m_m_u2->m_y*t_v2Y);
		t_impulse=-m_m_limitMass2*t_Cdot;
		t_oldImpulse=m_m_limitImpulse2;
		m_m_limitImpulse2=c_b2Math::m_Max(FLOAT(0.0),m_m_limitImpulse2+t_impulse);
		t_impulse=m_m_limitImpulse2-t_oldImpulse;
		t_P2X=-t_impulse*m_m_u2->m_x;
		t_P2Y=-t_impulse*m_m_u2->m_y;
		t_bB->m_m_linearVelocity->m_x+=t_bB->m_m_invMass*t_P2X;
		t_bB->m_m_linearVelocity->m_y+=t_bB->m_m_invMass*t_P2Y;
		t_bB->m_m_angularVelocity+=t_bB->m_m_invI*(t_r2X*t_P2Y-t_r2Y*t_P2X);
	}
}
bool c_b2PulleyJoint::p_SolvePositionConstraints(Float t_baumgarte){
	c_b2Body* t_bA=m_m_bodyA;
	c_b2Body* t_bB=m_m_bodyB;
	c_b2Mat22* t_tMat=0;
	Float t_s1X=m_m_ground->m_m_xf->m_position->m_x+m_m_groundAnchor1->m_x;
	Float t_s1Y=m_m_ground->m_m_xf->m_position->m_y+m_m_groundAnchor1->m_y;
	Float t_s2X=m_m_ground->m_m_xf->m_position->m_x+m_m_groundAnchor2->m_x;
	Float t_s2Y=m_m_ground->m_m_xf->m_position->m_y+m_m_groundAnchor2->m_y;
	Float t_r1X=FLOAT(.0);
	Float t_r1Y=FLOAT(.0);
	Float t_r2X=FLOAT(.0);
	Float t_r2Y=FLOAT(.0);
	Float t_p1X=FLOAT(.0);
	Float t_p1Y=FLOAT(.0);
	Float t_p2X=FLOAT(.0);
	Float t_p2Y=FLOAT(.0);
	Float t_length1=FLOAT(.0);
	Float t_length2=FLOAT(.0);
	Float t_C=FLOAT(.0);
	Float t_impulse=FLOAT(.0);
	Float t_oldImpulse=FLOAT(.0);
	Float t_oldLimitPositionImpulse=FLOAT(.0);
	Float t_tX=FLOAT(.0);
	Float t_linearError=FLOAT(0.0);
	if(m_m_state==2){
		t_tMat=t_bA->m_m_xf->m_R;
		t_r1X=m_m_localAnchor1->m_x-t_bA->m_m_sweep->m_localCenter->m_x;
		t_r1Y=m_m_localAnchor1->m_y-t_bA->m_m_sweep->m_localCenter->m_y;
		t_tX=t_tMat->m_col1->m_x*t_r1X+t_tMat->m_col2->m_x*t_r1Y;
		t_r1Y=t_tMat->m_col1->m_y*t_r1X+t_tMat->m_col2->m_y*t_r1Y;
		t_r1X=t_tX;
		t_tMat=t_bB->m_m_xf->m_R;
		t_r2X=m_m_localAnchor2->m_x-t_bB->m_m_sweep->m_localCenter->m_x;
		t_r2Y=m_m_localAnchor2->m_y-t_bB->m_m_sweep->m_localCenter->m_y;
		t_tX=t_tMat->m_col1->m_x*t_r2X+t_tMat->m_col2->m_x*t_r2Y;
		t_r2Y=t_tMat->m_col1->m_y*t_r2X+t_tMat->m_col2->m_y*t_r2Y;
		t_r2X=t_tX;
		t_p1X=t_bA->m_m_sweep->m_c->m_x+t_r1X;
		t_p1Y=t_bA->m_m_sweep->m_c->m_y+t_r1Y;
		t_p2X=t_bB->m_m_sweep->m_c->m_x+t_r2X;
		t_p2Y=t_bB->m_m_sweep->m_c->m_y+t_r2Y;
		m_m_u1->p_Set2(t_p1X-t_s1X,t_p1Y-t_s1Y);
		m_m_u2->p_Set2(t_p2X-t_s2X,t_p2Y-t_s2Y);
		t_length1=m_m_u1->p_Length();
		t_length2=m_m_u2->p_Length();
		if(t_length1>FLOAT(0.005)){
			m_m_u1->p_Multiply(FLOAT(1.0)/t_length1);
		}else{
			m_m_u1->p_SetZero();
		}
		if(t_length2>FLOAT(0.005)){
			m_m_u2->p_Multiply(FLOAT(1.0)/t_length2);
		}else{
			m_m_u2->p_SetZero();
		}
		t_C=m_m_constant-t_length1-m_m_ratio*t_length2;
		t_linearError=c_b2Math::m_Max(t_linearError,-t_C);
		t_C=c_b2Math::m_Clamp(t_C+FLOAT(0.005),FLOAT(-0.2),FLOAT(0.0));
		t_impulse=-m_m_pulleyMass*t_C;
		t_p1X=-t_impulse*m_m_u1->m_x;
		t_p1Y=-t_impulse*m_m_u1->m_y;
		t_p2X=-m_m_ratio*t_impulse*m_m_u2->m_x;
		t_p2Y=-m_m_ratio*t_impulse*m_m_u2->m_y;
		t_bA->m_m_sweep->m_c->m_x+=t_bA->m_m_invMass*t_p1X;
		t_bA->m_m_sweep->m_c->m_y+=t_bA->m_m_invMass*t_p1Y;
		t_bA->m_m_sweep->m_a+=t_bA->m_m_invI*(t_r1X*t_p1Y-t_r1Y*t_p1X);
		t_bB->m_m_sweep->m_c->m_x+=t_bB->m_m_invMass*t_p2X;
		t_bB->m_m_sweep->m_c->m_y+=t_bB->m_m_invMass*t_p2Y;
		t_bB->m_m_sweep->m_a+=t_bB->m_m_invI*(t_r2X*t_p2Y-t_r2Y*t_p2X);
		t_bA->p_SynchronizeTransform();
		t_bB->p_SynchronizeTransform();
	}
	if(m_m_limitState1==2){
		t_tMat=t_bA->m_m_xf->m_R;
		t_r1X=m_m_localAnchor1->m_x-t_bA->m_m_sweep->m_localCenter->m_x;
		t_r1Y=m_m_localAnchor1->m_y-t_bA->m_m_sweep->m_localCenter->m_y;
		t_tX=t_tMat->m_col1->m_x*t_r1X+t_tMat->m_col2->m_x*t_r1Y;
		t_r1Y=t_tMat->m_col1->m_y*t_r1X+t_tMat->m_col2->m_y*t_r1Y;
		t_r1X=t_tX;
		t_p1X=t_bA->m_m_sweep->m_c->m_x+t_r1X;
		t_p1Y=t_bA->m_m_sweep->m_c->m_y+t_r1Y;
		m_m_u1->p_Set2(t_p1X-t_s1X,t_p1Y-t_s1Y);
		t_length1=m_m_u1->p_Length();
		if(t_length1>FLOAT(0.005)){
			m_m_u1->m_x*=FLOAT(1.0)/t_length1;
			m_m_u1->m_y*=FLOAT(1.0)/t_length1;
		}else{
			m_m_u1->p_SetZero();
		}
		t_C=m_m_maxLength1-t_length1;
		t_linearError=c_b2Math::m_Max(t_linearError,-t_C);
		t_C=c_b2Math::m_Clamp(t_C+FLOAT(0.005),FLOAT(-0.2),FLOAT(0.0));
		t_impulse=-m_m_limitMass1*t_C;
		t_p1X=-t_impulse*m_m_u1->m_x;
		t_p1Y=-t_impulse*m_m_u1->m_y;
		t_bA->m_m_sweep->m_c->m_x+=t_bA->m_m_invMass*t_p1X;
		t_bA->m_m_sweep->m_c->m_y+=t_bA->m_m_invMass*t_p1Y;
		t_bA->m_m_sweep->m_a+=t_bA->m_m_invI*(t_r1X*t_p1Y-t_r1Y*t_p1X);
		t_bA->p_SynchronizeTransform();
	}
	if(m_m_limitState2==2){
		t_tMat=t_bB->m_m_xf->m_R;
		t_r2X=m_m_localAnchor2->m_x-t_bB->m_m_sweep->m_localCenter->m_x;
		t_r2Y=m_m_localAnchor2->m_y-t_bB->m_m_sweep->m_localCenter->m_y;
		t_tX=t_tMat->m_col1->m_x*t_r2X+t_tMat->m_col2->m_x*t_r2Y;
		t_r2Y=t_tMat->m_col1->m_y*t_r2X+t_tMat->m_col2->m_y*t_r2Y;
		t_r2X=t_tX;
		t_p2X=t_bB->m_m_sweep->m_c->m_x+t_r2X;
		t_p2Y=t_bB->m_m_sweep->m_c->m_y+t_r2Y;
		m_m_u2->p_Set2(t_p2X-t_s2X,t_p2Y-t_s2Y);
		t_length2=m_m_u2->p_Length();
		if(t_length2>FLOAT(0.005)){
			m_m_u2->m_x*=FLOAT(1.0)/t_length2;
			m_m_u2->m_y*=FLOAT(1.0)/t_length2;
		}else{
			m_m_u2->p_SetZero();
		}
		t_C=m_m_maxLength2-t_length2;
		t_linearError=c_b2Math::m_Max(t_linearError,-t_C);
		t_C=c_b2Math::m_Clamp(t_C+FLOAT(0.005),FLOAT(-0.2),FLOAT(0.0));
		t_impulse=-m_m_limitMass2*t_C;
		t_p2X=-t_impulse*m_m_u2->m_x;
		t_p2Y=-t_impulse*m_m_u2->m_y;
		t_bB->m_m_sweep->m_c->m_x+=t_bB->m_m_invMass*t_p2X;
		t_bB->m_m_sweep->m_c->m_y+=t_bB->m_m_invMass*t_p2Y;
		t_bB->m_m_sweep->m_a+=t_bB->m_m_invI*(t_r2X*t_p2Y-t_r2Y*t_p2X);
		t_bB->p_SynchronizeTransform();
	}
	return t_linearError<FLOAT(0.005);
}
void c_b2PulleyJoint::mark(){
	c_b2Joint::mark();
	gc_mark_q(m_m_ground);
	gc_mark_q(m_m_groundAnchor1);
	gc_mark_q(m_m_groundAnchor2);
	gc_mark_q(m_m_localAnchor1);
	gc_mark_q(m_m_localAnchor2);
	gc_mark_q(m_m_u1);
	gc_mark_q(m_m_u2);
}
c_b2PolyAndCircleContact::c_b2PolyAndCircleContact(){
}
void c_b2PolyAndCircleContact::p_Reset(c_b2Fixture* t_fixtureA,c_b2Fixture* t_fixtureB){
	c_b2Contact::p_Reset(t_fixtureA,t_fixtureB);
}
void c_b2PolyAndCircleContact::p_Evaluate(){
	c_b2Body* t_bA=m_m_fixtureA->m_m_body;
	c_b2Body* t_bB=m_m_fixtureB->m_m_body;
	c_b2Collision::m_CollidePolygonAndCircle(m_m_manifold,dynamic_cast<c_b2PolygonShape*>(m_m_fixtureA->p_GetShape()),t_bA->m_m_xf,dynamic_cast<c_b2CircleShape*>(m_m_fixtureB->p_GetShape()),t_bB->m_m_xf);
}
c_b2PolyAndCircleContact* c_b2PolyAndCircleContact::m_new(){
	c_b2Contact::m_new();
	return this;
}
void c_b2PolyAndCircleContact::mark(){
	c_b2Contact::mark();
}
int bb_graphics_DrawRect(Float t_x,Float t_y,Float t_w,Float t_h){
	bb_graphics_context->p_Validate();
	bb_graphics_renderDevice->DrawRect(t_x,t_y,t_w,t_h);
	return 0;
}
c_b2DynamicTreeNode::c_b2DynamicTreeNode(){
	m_parent=0;
	m_child1=0;
	m_child2=0;
	m_id=0;
	m_aabb=(new c_b2AABB)->m_new();
	m_userData=0;
}
int c_b2DynamicTreeNode::m_idCount;
c_b2DynamicTreeNode* c_b2DynamicTreeNode::m_new(){
	m_id=m_idCount;
	m_idCount+=1;
	return this;
}
void c_b2DynamicTreeNode::mark(){
	Object::mark();
	gc_mark_q(m_parent);
	gc_mark_q(m_child1);
	gc_mark_q(m_child2);
	gc_mark_q(m_aabb);
	gc_mark_q(m_userData);
}
c_b2DynamicTree::c_b2DynamicTree(){
	m_m_root=0;
	m_m_freeList=0;
	m_m_path=0;
	m_m_insertionCount=0;
	m_nodeStack=Array<c_b2DynamicTreeNode* >(128);
}
c_b2DynamicTree* c_b2DynamicTree::m_new(){
	m_m_root=0;
	m_m_freeList=0;
	m_m_path=0;
	m_m_insertionCount=0;
	return this;
}
c_b2DynamicTreeNode* c_b2DynamicTree::p_AllocateNode(){
	if((m_m_freeList)!=0){
		c_b2DynamicTreeNode* t_node=m_m_freeList;
		gc_assign(m_m_freeList,t_node->m_parent);
		t_node->m_parent=0;
		t_node->m_child1=0;
		t_node->m_child2=0;
		return t_node;
	}
	return (new c_b2DynamicTreeNode)->m_new();
}
c_b2Vec2* c_b2DynamicTree::m_shared_aabbCenter;
void c_b2DynamicTree::p_InsertLeaf(c_b2DynamicTreeNode* t_leaf){
	m_m_insertionCount+=1;
	if(m_m_root==0){
		gc_assign(m_m_root,t_leaf);
		m_m_root->m_parent=0;
		return;
	}
	t_leaf->m_aabb->p_GetCenter(m_shared_aabbCenter);
	Float t_centerX=m_shared_aabbCenter->m_x;
	Float t_centerY=m_shared_aabbCenter->m_y;
	c_b2DynamicTreeNode* t_sibling=m_m_root;
	if(t_sibling->m_child1!=0){
		do{
			c_b2DynamicTreeNode* t_child1=t_sibling->m_child1;
			c_b2DynamicTreeNode* t_child2=t_sibling->m_child2;
			c_b2AABB* t_aabb1=t_child1->m_aabb;
			c_b2AABB* t_aabb2=t_child2->m_aabb;
			c_b2Vec2* t_lowerBound=t_aabb1->m_lowerBound;
			c_b2Vec2* t_upperBound=t_aabb1->m_upperBound;
			Float t_midX=(t_lowerBound->m_x+t_upperBound->m_x)*FLOAT(0.5)-t_centerX;
			if(t_midX<FLOAT(0.0)){
				t_midX=-t_midX;
			}
			Float t_midY=(t_lowerBound->m_y+t_upperBound->m_y)*FLOAT(0.5)-t_centerY;
			if(t_midY<FLOAT(0.0)){
				t_midY=-t_midY;
			}
			Float t_norm1=t_midX+t_midY;
			t_lowerBound=t_aabb2->m_lowerBound;
			t_upperBound=t_aabb2->m_upperBound;
			t_midX=(t_lowerBound->m_x+t_upperBound->m_x)*FLOAT(0.5)-t_centerX;
			if(t_midX<FLOAT(0.0)){
				t_midX=-t_midX;
			}
			t_midY=(t_lowerBound->m_y+t_upperBound->m_y)*FLOAT(0.5)-t_centerY;
			if(t_midY<FLOAT(0.0)){
				t_midY=-t_midY;
			}
			Float t_norm2=t_midX+t_midY;
			if(t_norm1<t_norm2){
				t_sibling=t_child1;
			}else{
				t_sibling=t_child2;
			}
		}while(!(t_sibling->m_child1==0));
	}
	c_b2DynamicTreeNode* t_node1=t_sibling->m_parent;
	c_b2DynamicTreeNode* t_node2=p_AllocateNode();
	gc_assign(t_node2->m_parent,t_node1);
	t_node2->m_userData=0;
	t_node2->m_aabb->p_Combine(t_leaf->m_aabb,t_sibling->m_aabb);
	if((t_node1)!=0){
		if(t_sibling->m_parent->m_child1==t_sibling){
			gc_assign(t_node1->m_child1,t_node2);
		}else{
			gc_assign(t_node1->m_child2,t_node2);
		}
		gc_assign(t_node2->m_child1,t_sibling);
		gc_assign(t_node2->m_child2,t_leaf);
		gc_assign(t_sibling->m_parent,t_node2);
		gc_assign(t_leaf->m_parent,t_node2);
		do{
			if(t_node1->m_aabb->p_Contains2(t_node2->m_aabb)){
				break;
			}
			t_node1->m_aabb->p_Combine(t_node1->m_child1->m_aabb,t_node1->m_child2->m_aabb);
			t_node2=t_node1;
			t_node1=t_node1->m_parent;
		}while(!(t_node1==0));
	}else{
		gc_assign(t_node2->m_child1,t_sibling);
		gc_assign(t_node2->m_child2,t_leaf);
		gc_assign(t_sibling->m_parent,t_node2);
		gc_assign(t_leaf->m_parent,t_node2);
		gc_assign(m_m_root,t_node2);
	}
}
c_b2DynamicTreeNode* c_b2DynamicTree::p_CreateProxy(c_b2AABB* t_aabb,Object* t_userData){
	c_b2DynamicTreeNode* t_node=p_AllocateNode();
	Float t_extendX=FLOAT(0.1);
	Float t_extendY=FLOAT(0.1);
	t_node->m_aabb->m_lowerBound->m_x=t_aabb->m_lowerBound->m_x-t_extendX;
	t_node->m_aabb->m_lowerBound->m_y=t_aabb->m_lowerBound->m_y-t_extendY;
	t_node->m_aabb->m_upperBound->m_x=t_aabb->m_upperBound->m_x+t_extendX;
	t_node->m_aabb->m_upperBound->m_y=t_aabb->m_upperBound->m_y+t_extendY;
	gc_assign(t_node->m_userData,t_userData);
	p_InsertLeaf(t_node);
	return t_node;
}
void c_b2DynamicTree::p_FreeNode(c_b2DynamicTreeNode* t_node){
	gc_assign(t_node->m_parent,m_m_freeList);
	gc_assign(m_m_freeList,t_node);
}
void c_b2DynamicTree::p_RemoveLeaf(c_b2DynamicTreeNode* t_leaf){
	if(t_leaf==m_m_root){
		m_m_root=0;
		return;
	}
	c_b2DynamicTreeNode* t_node2=t_leaf->m_parent;
	c_b2DynamicTreeNode* t_node1=t_node2->m_parent;
	c_b2DynamicTreeNode* t_sibling=0;
	if(t_node2->m_child1==t_leaf){
		t_sibling=t_node2->m_child2;
	}else{
		t_sibling=t_node2->m_child1;
	}
	if((t_node1)!=0){
		if(t_node1->m_child1==t_node2){
			gc_assign(t_node1->m_child1,t_sibling);
		}else{
			gc_assign(t_node1->m_child2,t_sibling);
		}
		gc_assign(t_sibling->m_parent,t_node1);
		p_FreeNode(t_node2);
		while((t_node1)!=0){
			c_b2AABB* t_oldAABB=t_node1->m_aabb;
			gc_assign(t_node1->m_aabb,c_b2AABB::m_StaticCombine(t_node1->m_child1->m_aabb,t_node1->m_child2->m_aabb));
			if(t_oldAABB->p_Contains2(t_node1->m_aabb)){
				break;
			}
			t_node1=t_node1->m_parent;
		}
	}else{
		gc_assign(m_m_root,t_sibling);
		t_sibling->m_parent=0;
		p_FreeNode(t_node2);
	}
}
void c_b2DynamicTree::p_DestroyProxy3(c_b2DynamicTreeNode* t_proxy){
	p_RemoveLeaf(t_proxy);
	p_FreeNode(t_proxy);
}
bool c_b2DynamicTree::p_MoveProxy2(c_b2DynamicTreeNode* t_proxy,c_b2AABB* t_aabb,c_b2Vec2* t_displacement){
	if(t_proxy->m_aabb->p_Contains2(t_aabb)){
		return false;
	}
	p_RemoveLeaf(t_proxy);
	Float t_extendX=-t_displacement->m_x;
	if(t_displacement->m_x>FLOAT(0.0)){
		t_extendX=t_displacement->m_x;
	}
	t_extendX*=FLOAT(2.0);
	t_extendX+=FLOAT(0.1);
	Float t_extendY=-t_displacement->m_y;
	if(t_displacement->m_y>FLOAT(0.0)){
		t_extendY=t_displacement->m_y;
	}
	t_extendY*=FLOAT(2.0);
	t_extendY+=FLOAT(0.1);
	t_proxy->m_aabb->m_lowerBound->m_x=t_aabb->m_lowerBound->m_x-t_extendX;
	t_proxy->m_aabb->m_lowerBound->m_y=t_aabb->m_lowerBound->m_y-t_extendY;
	t_proxy->m_aabb->m_upperBound->m_x=t_aabb->m_upperBound->m_x+t_extendX;
	t_proxy->m_aabb->m_upperBound->m_y=t_aabb->m_upperBound->m_y+t_extendY;
	p_InsertLeaf(t_proxy);
	return true;
}
c_b2AABB* c_b2DynamicTree::p_GetFatAABB2(c_b2DynamicTreeNode* t_proxy){
	return t_proxy->m_aabb;
}
void c_b2DynamicTree::p_Query(c_QueryCallback* t_callback,c_b2AABB* t_aabb){
	if(m_m_root==0){
		return;
	}
	int t_count=0;
	int t_nodeStackLength=m_nodeStack.Length();
	gc_assign(m_nodeStack[t_count],m_m_root);
	t_count+=1;
	while(t_count>0){
		t_count-=1;
		c_b2DynamicTreeNode* t_node=m_nodeStack[t_count];
		bool t_overlap=true;
		c_b2Vec2* t_upperBound=t_node->m_aabb->m_upperBound;
		c_b2Vec2* t_otherLowerBound=t_aabb->m_lowerBound;
		if(t_otherLowerBound->m_x>t_upperBound->m_x){
			t_overlap=false;
		}else{
			if(t_otherLowerBound->m_y>t_upperBound->m_y){
				t_overlap=false;
			}else{
				c_b2Vec2* t_otherUpperBound=t_aabb->m_upperBound;
				c_b2Vec2* t_lowerBound=t_node->m_aabb->m_lowerBound;
				if(t_lowerBound->m_x>t_otherUpperBound->m_x){
					t_overlap=false;
				}else{
					if(t_lowerBound->m_y>t_otherUpperBound->m_y){
						t_overlap=false;
					}
				}
			}
		}
		if(t_overlap){
			if(t_node->m_child1==0){
				bool t_proceed=t_callback->p_Callback2(t_node);
				if(!t_proceed){
					return;
				}
			}else{
				if(t_count+2>=t_nodeStackLength){
					gc_assign(m_nodeStack,m_nodeStack.Resize(t_count*2));
					t_nodeStackLength=t_count*2;
				}
				gc_assign(m_nodeStack[t_count],t_node->m_child1);
				t_count+=1;
				gc_assign(m_nodeStack[t_count],t_node->m_child2);
				t_count+=1;
			}
		}
	}
}
Object* c_b2DynamicTree::p_GetUserData(c_b2DynamicTreeNode* t_proxy){
	return t_proxy->m_userData;
}
void c_b2DynamicTree::mark(){
	Object::mark();
	gc_mark_q(m_m_root);
	gc_mark_q(m_m_freeList);
	gc_mark_q(m_nodeStack);
}
c_FlashArray3::c_FlashArray3(){
	m_length=0;
	m_arrLength=100;
	m_arr=Array<c_b2DynamicTreeNode* >(100);
	m_EmptyArr=Array<c_b2DynamicTreeNode* >(0);
}
int c_FlashArray3::p_Length(){
	return m_length;
}
void c_FlashArray3::p_Length2(int t_value){
	m_length=t_value;
	if(m_length>m_arrLength){
		m_arrLength=m_length;
		gc_assign(m_arr,m_arr.Resize(m_length));
	}
}
c_FlashArray3* c_FlashArray3::m_new(int t_length){
	p_Length2(t_length);
	return this;
}
c_FlashArray3* c_FlashArray3::m_new2(Array<c_b2DynamicTreeNode* > t_vals){
	gc_assign(m_arr,t_vals);
	m_arrLength=m_arr.Length();
	m_length=m_arrLength;
	return this;
}
c_FlashArray3* c_FlashArray3::m_new3(){
	return this;
}
void c_FlashArray3::p_Set12(int t_index,c_b2DynamicTreeNode* t_item){
	if(t_index>=m_arrLength){
		m_arrLength=t_index+100;
		gc_assign(m_arr,m_arr.Resize(m_arrLength));
	}
	gc_assign(m_arr[t_index],t_item);
	if(t_index>=m_length){
		m_length=t_index+1;
	}
}
int c_FlashArray3::p_IndexOf(c_b2DynamicTreeNode* t_element){
	for(int t_index=0;t_index<m_length;t_index=t_index+1){
		c_b2DynamicTreeNode* t_check=m_arr[t_index];
		if(t_check==t_element){
			return t_index;
		}
	}
	return -1;
}
void c_FlashArray3::p_Splice(int t_index,int t_deletes,Array<c_b2DynamicTreeNode* > t_insert){
	if(t_deletes==-1){
		t_deletes=m_length-t_index;
	}
	int t_newLength=m_length-t_deletes;
	if(t_newLength<0){
		t_newLength=0;
	}
	t_newLength+=t_insert.Length();
	Array<c_b2DynamicTreeNode* > t_newArr=Array<c_b2DynamicTreeNode* >();
	if(t_index>0){
		t_newArr=m_arr.Slice(0,t_index-1);
		t_newArr=t_newArr.Resize(t_newLength);
	}else{
		t_index=0;
		t_newArr=Array<c_b2DynamicTreeNode* >(t_newLength);
	}
	int t_copyInd=t_index;
	if((t_insert).Length()!=0){
		Array<c_b2DynamicTreeNode* > t_=t_insert;
		int t_2=0;
		while(t_2<t_.Length()){
			c_b2DynamicTreeNode* t_val=t_[t_2];
			t_2=t_2+1;
			gc_assign(t_newArr[t_copyInd],t_val);
			t_copyInd+=1;
		}
	}
	for(int t_i=t_index+t_deletes;t_i<p_Length();t_i=t_i+1){
		gc_assign(t_newArr[t_copyInd],m_arr[t_i]);
		t_copyInd+=1;
	}
	gc_assign(m_arr,t_newArr);
	m_arrLength=t_newLength;
	m_length=t_newLength;
}
void c_FlashArray3::p_Splice2(int t_index,int t_deletes,c_b2DynamicTreeNode* t_insert){
	c_b2DynamicTreeNode* t_[]={t_insert};
	p_Splice(t_index,t_deletes,Array<c_b2DynamicTreeNode* >(t_,1));
}
void c_FlashArray3::p_Splice3(int t_index,int t_deletes){
	p_Splice(t_index,t_deletes,m_EmptyArr);
}
Array<c_b2DynamicTreeNode* > c_FlashArray3::p_BackingArray(){
	return m_arr;
}
void c_FlashArray3::mark(){
	Object::mark();
	gc_mark_q(m_arr);
	gc_mark_q(m_EmptyArr);
}
c_QueryCallback::c_QueryCallback(){
}
c_QueryCallback* c_QueryCallback::m_new(){
	return this;
}
void c_QueryCallback::mark(){
	Object::mark();
}
c_TreeQueryCallback::c_TreeQueryCallback(){
}
c_TreeQueryCallback* c_TreeQueryCallback::m_new(){
	c_QueryCallback::m_new();
	return this;
}
void c_TreeQueryCallback::mark(){
	c_QueryCallback::mark();
}
c_DTQueryCallback::c_DTQueryCallback(){
	m_m_pairCount=0;
	m_queryProxy=0;
	m_m_pairBuffer=(new c_FlashArray4)->m_new3();
}
c_DTQueryCallback* c_DTQueryCallback::m_new(){
	c_TreeQueryCallback::m_new();
	return this;
}
bool c_DTQueryCallback::p_Callback2(Object* t_a){
	c_b2DynamicTreeNode* t_proxy=dynamic_cast<c_b2DynamicTreeNode*>(t_a);
	if(t_proxy==m_queryProxy){
		return true;
	}
	if(m_m_pairCount==m_m_pairBuffer->p_Length()){
		m_m_pairBuffer->p_Set13(m_m_pairCount,(new c_b2DynamicTreePair)->m_new());
	}
	c_b2DynamicTreePair* t_pair=m_m_pairBuffer->p_Get(m_m_pairCount);
	if(t_proxy->m_id<m_queryProxy->m_id){
		gc_assign(t_pair->m_proxyA,t_proxy);
		gc_assign(t_pair->m_proxyB,m_queryProxy);
	}else{
		gc_assign(t_pair->m_proxyA,m_queryProxy);
		gc_assign(t_pair->m_proxyB,t_proxy);
	}
	m_m_pairCount+=1;
	return true;
}
void c_DTQueryCallback::mark(){
	c_TreeQueryCallback::mark();
	gc_mark_q(m_queryProxy);
	gc_mark_q(m_m_pairBuffer);
}
c_b2DynamicTreePair::c_b2DynamicTreePair(){
	m_proxyA=0;
	m_proxyB=0;
}
c_b2DynamicTreePair* c_b2DynamicTreePair::m_new(){
	return this;
}
void c_b2DynamicTreePair::mark(){
	Object::mark();
	gc_mark_q(m_proxyA);
	gc_mark_q(m_proxyB);
}
c_FlashArray4::c_FlashArray4(){
	m_length=0;
	m_arrLength=100;
	m_arr=Array<c_b2DynamicTreePair* >(100);
}
int c_FlashArray4::p_Length(){
	return m_length;
}
void c_FlashArray4::p_Length2(int t_value){
	m_length=t_value;
	if(m_length>m_arrLength){
		m_arrLength=m_length;
		gc_assign(m_arr,m_arr.Resize(m_length));
	}
}
c_FlashArray4* c_FlashArray4::m_new(int t_length){
	p_Length2(t_length);
	return this;
}
c_FlashArray4* c_FlashArray4::m_new2(Array<c_b2DynamicTreePair* > t_vals){
	gc_assign(m_arr,t_vals);
	m_arrLength=m_arr.Length();
	m_length=m_arrLength;
	return this;
}
c_FlashArray4* c_FlashArray4::m_new3(){
	return this;
}
c_b2DynamicTreePair* c_FlashArray4::p_Get(int t_index){
	if(t_index>=0 && m_length>t_index){
		return m_arr[t_index];
	}else{
		return 0;
	}
}
void c_FlashArray4::p_Set13(int t_index,c_b2DynamicTreePair* t_item){
	if(t_index>=m_arrLength){
		m_arrLength=t_index+100;
		gc_assign(m_arr,m_arr.Resize(m_arrLength));
	}
	gc_assign(m_arr[t_index],t_item);
	if(t_index>=m_length){
		m_length=t_index+1;
	}
}
void c_FlashArray4::mark(){
	Object::mark();
	gc_mark_q(m_arr);
}
c_b2Collision::c_b2Collision(){
}
void c_b2Collision::m_CollidePolygonAndCircle(c_b2Manifold* t_manifold,c_b2PolygonShape* t_polygon,c_b2Transform* t_xf1,c_b2CircleShape* t_circle,c_b2Transform* t_xf2){
	t_manifold->m_m_pointCount=0;
	c_b2ManifoldPoint* t_tPoint=0;
	Float t_dX=FLOAT(.0);
	Float t_dY=FLOAT(.0);
	Float t_positionX=FLOAT(.0);
	Float t_positionY=FLOAT(.0);
	c_b2Vec2* t_tVec=0;
	c_b2Mat22* t_tMat=0;
	t_tMat=t_xf2->m_R;
	t_tVec=t_circle->m_m_p;
	Float t_cX=t_xf2->m_position->m_x+(t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y);
	Float t_cY=t_xf2->m_position->m_y+(t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y);
	t_dX=t_cX-t_xf1->m_position->m_x;
	t_dY=t_cY-t_xf1->m_position->m_y;
	t_tMat=t_xf1->m_R;
	Float t_cLocalX=t_dX*t_tMat->m_col1->m_x+t_dY*t_tMat->m_col1->m_y;
	Float t_cLocalY=t_dX*t_tMat->m_col2->m_x+t_dY*t_tMat->m_col2->m_y;
	Float t_dist=FLOAT(.0);
	int t_normalIndex=0;
	Float t_separation=FLOAT(-3.4e38);
	Float t_radius=t_polygon->m_m_radius+t_circle->m_m_radius;
	int t_vertexCount=t_polygon->m_m_vertexCount;
	Array<c_b2Vec2* > t_vertices=t_polygon->m_m_vertices;
	Array<c_b2Vec2* > t_normals=t_polygon->m_m_normals;
	for(int t_i=0;t_i<t_vertexCount;t_i=t_i+1){
		t_tVec=t_vertices[t_i];
		t_dX=t_cLocalX-t_tVec->m_x;
		t_dY=t_cLocalY-t_tVec->m_y;
		t_tVec=t_normals[t_i];
		Float t_s=t_tVec->m_x*t_dX+t_tVec->m_y*t_dY;
		if(t_s>t_radius){
			return;
		}
		if(t_s>t_separation){
			t_separation=t_s;
			t_normalIndex=t_i;
		}
	}
	int t_vertIndex1=t_normalIndex;
	int t_vertIndex2=0;
	if(t_vertIndex1+1<t_vertexCount){
		t_vertIndex2=t_vertIndex1+1;
	}
	c_b2Vec2* t_v1=t_vertices[t_vertIndex1];
	c_b2Vec2* t_v2=t_vertices[t_vertIndex2];
	if(t_separation<FLOAT(1e-15)){
		t_manifold->m_m_pointCount=1;
		t_manifold->m_m_type=2;
		t_manifold->m_m_localPlaneNormal->p_SetV(t_normals[t_normalIndex]);
		t_manifold->m_m_localPoint->m_x=FLOAT(0.5)*(t_v1->m_x+t_v2->m_x);
		t_manifold->m_m_localPoint->m_y=FLOAT(0.5)*(t_v1->m_y+t_v2->m_y);
		t_manifold->m_m_points[0]->m_m_localPoint->p_SetV(t_circle->m_m_p);
		t_manifold->m_m_points[0]->m_m_id->p_Key2(0);
		return;
	}
	Float t_u1=(t_cLocalX-t_v1->m_x)*(t_v2->m_x-t_v1->m_x)+(t_cLocalY-t_v1->m_y)*(t_v2->m_y-t_v1->m_y);
	Float t_u2=(t_cLocalX-t_v2->m_x)*(t_v1->m_x-t_v2->m_x)+(t_cLocalY-t_v2->m_y)*(t_v1->m_y-t_v2->m_y);
	if(t_u1<=FLOAT(0.0)){
		if((t_cLocalX-t_v1->m_x)*(t_cLocalX-t_v1->m_x)+(t_cLocalY-t_v1->m_y)*(t_cLocalY-t_v1->m_y)>t_radius*t_radius){
			return;
		}
		t_manifold->m_m_pointCount=1;
		t_manifold->m_m_type=2;
		t_manifold->m_m_localPlaneNormal->m_x=t_cLocalX-t_v1->m_x;
		t_manifold->m_m_localPlaneNormal->m_y=t_cLocalY-t_v1->m_y;
		t_manifold->m_m_localPlaneNormal->p_Normalize();
		t_manifold->m_m_localPoint->p_SetV(t_v1);
		t_manifold->m_m_points[0]->m_m_localPoint->p_SetV(t_circle->m_m_p);
		t_manifold->m_m_points[0]->m_m_id->p_Key2(0);
	}else{
		if(t_u2<=FLOAT(0.0)){
			if((t_cLocalX-t_v2->m_x)*(t_cLocalX-t_v2->m_x)+(t_cLocalY-t_v2->m_y)*(t_cLocalY-t_v2->m_y)>t_radius*t_radius){
				return;
			}
			t_manifold->m_m_pointCount=1;
			t_manifold->m_m_type=2;
			t_manifold->m_m_localPlaneNormal->m_x=t_cLocalX-t_v2->m_x;
			t_manifold->m_m_localPlaneNormal->m_y=t_cLocalY-t_v2->m_y;
			t_manifold->m_m_localPlaneNormal->p_Normalize();
			t_manifold->m_m_localPoint->p_SetV(t_v2);
			t_manifold->m_m_points[0]->m_m_localPoint->p_SetV(t_circle->m_m_p);
			t_manifold->m_m_points[0]->m_m_id->p_Key2(0);
		}else{
			Float t_faceCenterX=FLOAT(0.5)*(t_v1->m_x+t_v2->m_x);
			Float t_faceCenterY=FLOAT(0.5)*(t_v1->m_y+t_v2->m_y);
			t_separation=(t_cLocalX-t_faceCenterX)*t_normals[t_vertIndex1]->m_x+(t_cLocalY-t_faceCenterY)*t_normals[t_vertIndex1]->m_y;
			if(t_separation>t_radius){
				return;
			}
			t_manifold->m_m_pointCount=1;
			t_manifold->m_m_type=2;
			t_manifold->m_m_localPlaneNormal->m_x=t_normals[t_vertIndex1]->m_x;
			t_manifold->m_m_localPlaneNormal->m_y=t_normals[t_vertIndex1]->m_y;
			t_manifold->m_m_localPlaneNormal->p_Normalize();
			t_manifold->m_m_localPoint->p_Set2(t_faceCenterX,t_faceCenterY);
			t_manifold->m_m_points[0]->m_m_localPoint->p_SetV(t_circle->m_m_p);
			t_manifold->m_m_points[0]->m_m_id->p_Key2(0);
		}
	}
}
void c_b2Collision::m_CollideCircles(c_b2Manifold* t_manifold,c_b2CircleShape* t_circle1,c_b2Transform* t_xf1,c_b2CircleShape* t_circle2,c_b2Transform* t_xf2){
	t_manifold->m_m_pointCount=0;
	c_b2Mat22* t_tMat=0;
	c_b2Vec2* t_tVec=0;
	t_tMat=t_xf1->m_R;
	t_tVec=t_circle1->m_m_p;
	Float t_p1X=t_xf1->m_position->m_x+(t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y);
	Float t_p1Y=t_xf1->m_position->m_y+(t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y);
	t_tMat=t_xf2->m_R;
	t_tVec=t_circle2->m_m_p;
	Float t_p2X=t_xf2->m_position->m_x+(t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y);
	Float t_p2Y=t_xf2->m_position->m_y+(t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y);
	Float t_dX=t_p2X-t_p1X;
	Float t_dY=t_p2Y-t_p1Y;
	Float t_distSqr=t_dX*t_dX+t_dY*t_dY;
	Float t_radius=t_circle1->m_m_radius+t_circle2->m_m_radius;
	if(t_distSqr>t_radius*t_radius){
		return;
	}
	t_manifold->m_m_type=1;
	t_manifold->m_m_localPoint->p_SetV(t_circle1->m_m_p);
	t_manifold->m_m_localPlaneNormal->p_SetZero();
	t_manifold->m_m_pointCount=1;
	t_manifold->m_m_points[0]->m_m_localPoint->p_SetV(t_circle2->m_m_p);
	t_manifold->m_m_points[0]->m_m_id->p_Key2(0);
}
Array<int > c_b2Collision::m_s_edgeAO;
Float c_b2Collision::m_EdgeSeparation(c_b2PolygonShape* t_poly1,c_b2Transform* t_xf1,int t_edge1,c_b2PolygonShape* t_poly2,c_b2Transform* t_xf2){
	int t_count1=t_poly1->m_m_vertexCount;
	Array<c_b2Vec2* > t_vertices1=t_poly1->m_m_vertices;
	Array<c_b2Vec2* > t_normals1=t_poly1->m_m_normals;
	int t_count2=t_poly2->m_m_vertexCount;
	Array<c_b2Vec2* > t_vertices2=t_poly2->m_m_vertices;
	c_b2Mat22* t_tMat=0;
	c_b2Vec2* t_tVec=0;
	t_tMat=t_xf1->m_R;
	t_tVec=t_normals1[t_edge1];
	Float t_normal1WorldX=t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y;
	Float t_normal1WorldY=t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y;
	t_tMat=t_xf2->m_R;
	Float t_normal1X=t_tMat->m_col1->m_x*t_normal1WorldX+t_tMat->m_col1->m_y*t_normal1WorldY;
	Float t_normal1Y=t_tMat->m_col2->m_x*t_normal1WorldX+t_tMat->m_col2->m_y*t_normal1WorldY;
	int t_index=0;
	Float t_minDot=FLOAT(3.4e38);
	for(int t_i=0;t_i<t_count2;t_i=t_i+1){
		t_tVec=t_vertices2[t_i];
		Float t_dot=t_tVec->m_x*t_normal1X+t_tVec->m_y*t_normal1Y;
		if(t_dot<t_minDot){
			t_minDot=t_dot;
			t_index=t_i;
		}
	}
	t_tVec=t_vertices1[t_edge1];
	t_tMat=t_xf1->m_R;
	Float t_v1X=t_xf1->m_position->m_x+(t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y);
	Float t_v1Y=t_xf1->m_position->m_y+(t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y);
	t_tVec=t_vertices2[t_index];
	t_tMat=t_xf2->m_R;
	Float t_v2X=t_xf2->m_position->m_x+(t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y);
	Float t_v2Y=t_xf2->m_position->m_y+(t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y);
	t_v2X-=t_v1X;
	t_v2Y-=t_v1Y;
	Float t_separation=t_v2X*t_normal1WorldX+t_v2Y*t_normal1WorldY;
	return t_separation;
}
Float c_b2Collision::m_FindMaxSeparation(Array<int > t_edgeIndex,c_b2PolygonShape* t_poly1,c_b2Transform* t_xf1,c_b2PolygonShape* t_poly2,c_b2Transform* t_xf2){
	int t_count1=t_poly1->m_m_vertexCount;
	Array<c_b2Vec2* > t_normals1=t_poly1->m_m_normals;
	c_b2Vec2* t_tVec=0;
	c_b2Mat22* t_tMat=0;
	t_tMat=t_xf2->m_R;
	t_tVec=t_poly2->m_m_centroid;
	Float t_dX=t_xf2->m_position->m_x+(t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y);
	Float t_dY=t_xf2->m_position->m_y+(t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y);
	t_tMat=t_xf1->m_R;
	t_tVec=t_poly1->m_m_centroid;
	t_dX-=t_xf1->m_position->m_x+(t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y);
	t_dY-=t_xf1->m_position->m_y+(t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y);
	Float t_dLocal1X=t_dX*t_xf1->m_R->m_col1->m_x+t_dY*t_xf1->m_R->m_col1->m_y;
	Float t_dLocal1Y=t_dX*t_xf1->m_R->m_col2->m_x+t_dY*t_xf1->m_R->m_col2->m_y;
	int t_edge=0;
	Float t_maxDot=FLOAT(-3.4e38);
	for(int t_i=0;t_i<t_count1;t_i=t_i+1){
		t_tVec=t_normals1[t_i];
		Float t_dot=t_tVec->m_x*t_dLocal1X+t_tVec->m_y*t_dLocal1Y;
		if(t_dot>t_maxDot){
			t_maxDot=t_dot;
			t_edge=t_i;
		}
	}
	Float t_s=m_EdgeSeparation(t_poly1,t_xf1,t_edge,t_poly2,t_xf2);
	int t_prevEdge=t_count1-1;
	if(t_edge-1>=0){
		t_prevEdge=t_edge-1;
	}
	Float t_sPrev=m_EdgeSeparation(t_poly1,t_xf1,t_prevEdge,t_poly2,t_xf2);
	int t_nextEdge=0;
	if(t_edge+1<t_count1){
		t_nextEdge=t_edge+1;
	}
	Float t_sNext=m_EdgeSeparation(t_poly1,t_xf1,t_nextEdge,t_poly2,t_xf2);
	int t_bestEdge=0;
	Float t_bestSeparation=FLOAT(.0);
	int t_increment=0;
	if(t_sPrev>t_s && t_sPrev>t_sNext){
		t_increment=-1;
		t_bestEdge=t_prevEdge;
		t_bestSeparation=t_sPrev;
	}else{
		if(t_sNext>t_s){
			t_increment=1;
			t_bestEdge=t_nextEdge;
			t_bestSeparation=t_sNext;
		}else{
			t_edgeIndex[0]=t_edge;
			return t_s;
		}
	}
	while(true){
		if(t_increment==-1){
			if(t_bestEdge-1>=0){
				t_edge=t_bestEdge-1;
			}else{
				t_edge=t_count1-1;
			}
		}else{
			if(t_bestEdge+1<t_count1){
				t_edge=t_bestEdge+1;
			}else{
				t_edge=0;
			}
		}
		t_s=m_EdgeSeparation(t_poly1,t_xf1,t_edge,t_poly2,t_xf2);
		if(t_s>t_bestSeparation){
			t_bestEdge=t_edge;
			t_bestSeparation=t_s;
		}else{
			break;
		}
	}
	t_edgeIndex[0]=t_bestEdge;
	return t_bestSeparation;
}
Array<int > c_b2Collision::m_s_edgeBO;
Array<c_ClipVertex* > c_b2Collision::m_MakeClipPointVector(){
	c_ClipVertex* t_[]={(new c_ClipVertex)->m_new(),(new c_ClipVertex)->m_new()};
	return Array<c_ClipVertex* >(t_,2);
}
Array<c_ClipVertex* > c_b2Collision::m_s_incidentEdge;
void c_b2Collision::m_FindIncidentEdge(Array<c_ClipVertex* > t_c,c_b2PolygonShape* t_poly1,c_b2Transform* t_xf1,int t_edge1,c_b2PolygonShape* t_poly2,c_b2Transform* t_xf2){
	int t_count1=t_poly1->m_m_vertexCount;
	Array<c_b2Vec2* > t_normals1=t_poly1->m_m_normals;
	int t_count2=t_poly2->m_m_vertexCount;
	Array<c_b2Vec2* > t_vertices2=t_poly2->m_m_vertices;
	Array<c_b2Vec2* > t_normals2=t_poly2->m_m_normals;
	c_b2Mat22* t_tMat=0;
	c_b2Vec2* t_tVec=0;
	t_tMat=t_xf1->m_R;
	t_tVec=t_normals1[t_edge1];
	Float t_normal1X=t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y;
	Float t_normal1Y=t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y;
	t_tMat=t_xf2->m_R;
	Float t_tX=t_tMat->m_col1->m_x*t_normal1X+t_tMat->m_col1->m_y*t_normal1Y;
	t_normal1Y=t_tMat->m_col2->m_x*t_normal1X+t_tMat->m_col2->m_y*t_normal1Y;
	t_normal1X=t_tX;
	int t_index=0;
	Float t_minDot=FLOAT(3.4e38);
	for(int t_i=0;t_i<t_count2;t_i=t_i+1){
		t_tVec=t_normals2[t_i];
		Float t_dot=t_normal1X*t_tVec->m_x+t_normal1Y*t_tVec->m_y;
		if(t_dot<t_minDot){
			t_minDot=t_dot;
			t_index=t_i;
		}
	}
	c_ClipVertex* t_tClip=0;
	int t_i1=t_index;
	int t_i2=0;
	if(t_i1+1<t_count2){
		t_i2=t_i1+1;
	}
	t_tClip=t_c[0];
	t_tVec=t_vertices2[t_i1];
	t_tMat=t_xf2->m_R;
	t_tClip->m_v->m_x=t_xf2->m_position->m_x+(t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y);
	t_tClip->m_v->m_y=t_xf2->m_position->m_y+(t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y);
	t_tClip->m_id->m_features->p_ReferenceEdge2(t_edge1);
	t_tClip->m_id->m_features->p_IncidentEdge2(t_i1);
	t_tClip->m_id->m_features->p_IncidentVertex2(0);
	t_tClip=t_c[1];
	t_tVec=t_vertices2[t_i2];
	t_tMat=t_xf2->m_R;
	t_tClip->m_v->m_x=t_xf2->m_position->m_x+(t_tMat->m_col1->m_x*t_tVec->m_x+t_tMat->m_col2->m_x*t_tVec->m_y);
	t_tClip->m_v->m_y=t_xf2->m_position->m_y+(t_tMat->m_col1->m_y*t_tVec->m_x+t_tMat->m_col2->m_y*t_tVec->m_y);
	t_tClip->m_id->m_features->p_ReferenceEdge2(t_edge1);
	t_tClip->m_id->m_features->p_IncidentEdge2(t_i2);
	t_tClip->m_id->m_features->p_IncidentVertex2(1);
}
c_b2Vec2* c_b2Collision::m_s_localTangent;
c_b2Vec2* c_b2Collision::m_s_localNormal;
c_b2Vec2* c_b2Collision::m_s_planePoint;
c_b2Vec2* c_b2Collision::m_s_tangent;
c_b2Vec2* c_b2Collision::m_s_tangent2;
c_b2Vec2* c_b2Collision::m_s_normal;
c_b2Vec2* c_b2Collision::m_s_v11;
c_b2Vec2* c_b2Collision::m_s_v12;
Array<c_ClipVertex* > c_b2Collision::m_s_clipPoints1;
Array<c_ClipVertex* > c_b2Collision::m_s_clipPoints2;
int c_b2Collision::m_ClipSegmentToLine(Array<c_ClipVertex* > t_vOut,Array<c_ClipVertex* > t_vIn,c_b2Vec2* t_normal,Float t_offset){
	c_ClipVertex* t_cv=0;
	int t_numOut=0;
	t_cv=t_vIn[0];
	c_b2Vec2* t_vIn0=t_cv->m_v;
	t_cv=t_vIn[1];
	c_b2Vec2* t_vIn1=t_cv->m_v;
	Float t_distance0=t_normal->m_x*t_vIn0->m_x+t_normal->m_y*t_vIn0->m_y-t_offset;
	Float t_distance1=t_normal->m_x*t_vIn1->m_x+t_normal->m_y*t_vIn1->m_y-t_offset;
	if(t_distance0<=FLOAT(0.0)){
		t_vOut[t_numOut]->p_Set14(t_vIn[0]);
		t_numOut+=1;
	}
	if(t_distance1<=FLOAT(0.0)){
		t_vOut[t_numOut]->p_Set14(t_vIn[1]);
		t_numOut+=1;
	}
	if(t_distance0*t_distance1<FLOAT(0.0)){
		Float t_interp=t_distance0/(t_distance0-t_distance1);
		t_cv=t_vOut[t_numOut];
		c_b2Vec2* t_tVec=t_cv->m_v;
		t_tVec->m_x=t_vIn0->m_x+t_interp*(t_vIn1->m_x-t_vIn0->m_x);
		t_tVec->m_y=t_vIn0->m_y+t_interp*(t_vIn1->m_y-t_vIn0->m_y);
		t_cv=t_vOut[t_numOut];
		c_ClipVertex* t_cv2=0;
		if(t_distance0>FLOAT(0.0)){
			t_cv2=t_vIn[0];
			gc_assign(t_cv->m_id,t_cv2->m_id);
		}else{
			t_cv2=t_vIn[1];
			gc_assign(t_cv->m_id,t_cv2->m_id);
		}
		t_numOut+=1;
	}
	return t_numOut;
}
void c_b2Collision::m_CollidePolygons(c_b2Manifold* t_manifold,c_b2PolygonShape* t_polyA,c_b2Transform* t_xfA,c_b2PolygonShape* t_polyB,c_b2Transform* t_xfB){
	c_ClipVertex* t_cv=0;
	t_manifold->m_m_pointCount=0;
	Float t_totalRadius=t_polyA->m_m_radius+t_polyB->m_m_radius;
	int t_edgeA=0;
	m_s_edgeAO[0]=t_edgeA;
	Float t_separationA=m_FindMaxSeparation(m_s_edgeAO,t_polyA,t_xfA,t_polyB,t_xfB);
	t_edgeA=m_s_edgeAO[0];
	if(t_separationA>t_totalRadius){
		return;
	}
	int t_edgeB=0;
	m_s_edgeBO[0]=t_edgeB;
	Float t_separationB=m_FindMaxSeparation(m_s_edgeBO,t_polyB,t_xfB,t_polyA,t_xfA);
	t_edgeB=m_s_edgeBO[0];
	if(t_separationB>t_totalRadius){
		return;
	}
	c_b2PolygonShape* t_poly1=0;
	c_b2PolygonShape* t_poly2=0;
	c_b2Transform* t_xf1=0;
	c_b2Transform* t_xf2=0;
	int t_edge1=0;
	int t_flip=0;
	c_b2Mat22* t_tMat=0;
	if(t_separationB>FLOAT(0.98)*t_separationA+FLOAT(0.001)){
		t_poly1=t_polyB;
		t_poly2=t_polyA;
		t_xf1=t_xfB;
		t_xf2=t_xfA;
		t_edge1=t_edgeB;
		t_manifold->m_m_type=4;
		t_flip=1;
	}else{
		t_poly1=t_polyA;
		t_poly2=t_polyB;
		t_xf1=t_xfA;
		t_xf2=t_xfB;
		t_edge1=t_edgeA;
		t_manifold->m_m_type=2;
		t_flip=0;
	}
	Array<c_ClipVertex* > t_incidentEdge=m_s_incidentEdge;
	m_FindIncidentEdge(t_incidentEdge,t_poly1,t_xf1,t_edge1,t_poly2,t_xf2);
	int t_count1=t_poly1->m_m_vertexCount;
	Array<c_b2Vec2* > t_vertices1=t_poly1->m_m_vertices;
	c_b2Vec2* t_local_v11=t_vertices1[t_edge1];
	c_b2Vec2* t_local_v12=0;
	if(t_edge1+1<t_count1){
		t_local_v12=t_vertices1[t_edge1+1];
	}else{
		t_local_v12=t_vertices1[0];
	}
	c_b2Vec2* t_localTangent=m_s_localTangent;
	t_localTangent->m_x=t_local_v12->m_x-t_local_v11->m_x;
	t_localTangent->m_y=t_local_v12->m_y-t_local_v11->m_y;
	t_localTangent->p_Normalize();
	c_b2Vec2* t_localNormal=m_s_localNormal;
	t_localNormal->m_x=t_localTangent->m_y;
	t_localNormal->m_y=-t_localTangent->m_x;
	c_b2Vec2* t_planePoint=m_s_planePoint;
	t_planePoint->m_x=FLOAT(0.5)*(t_local_v11->m_x+t_local_v12->m_x);
	t_planePoint->m_y=FLOAT(0.5)*(t_local_v11->m_y+t_local_v12->m_y);
	c_b2Vec2* t_tangent=m_s_tangent;
	t_tMat=t_xf1->m_R;
	t_tangent->m_x=t_tMat->m_col1->m_x*t_localTangent->m_x+t_tMat->m_col2->m_x*t_localTangent->m_y;
	t_tangent->m_y=t_tMat->m_col1->m_y*t_localTangent->m_x+t_tMat->m_col2->m_y*t_localTangent->m_y;
	c_b2Vec2* t_tangent2=m_s_tangent2;
	t_tangent2->m_x=-t_tangent->m_x;
	t_tangent2->m_y=-t_tangent->m_y;
	c_b2Vec2* t_normal=m_s_normal;
	t_normal->m_x=t_tangent->m_y;
	t_normal->m_y=-t_tangent->m_x;
	c_b2Vec2* t_v11=m_s_v11;
	c_b2Vec2* t_v12=m_s_v12;
	t_v11->m_x=t_xf1->m_position->m_x+(t_tMat->m_col1->m_x*t_local_v11->m_x+t_tMat->m_col2->m_x*t_local_v11->m_y);
	t_v11->m_y=t_xf1->m_position->m_y+(t_tMat->m_col1->m_y*t_local_v11->m_x+t_tMat->m_col2->m_y*t_local_v11->m_y);
	t_v12->m_x=t_xf1->m_position->m_x+(t_tMat->m_col1->m_x*t_local_v12->m_x+t_tMat->m_col2->m_x*t_local_v12->m_y);
	t_v12->m_y=t_xf1->m_position->m_y+(t_tMat->m_col1->m_y*t_local_v12->m_x+t_tMat->m_col2->m_y*t_local_v12->m_y);
	Float t_frontOffset=t_normal->m_x*t_v11->m_x+t_normal->m_y*t_v11->m_y;
	Float t_sideOffset1=-t_tangent->m_x*t_v11->m_x-t_tangent->m_y*t_v11->m_y+t_totalRadius;
	Float t_sideOffset2=t_tangent->m_x*t_v12->m_x+t_tangent->m_y*t_v12->m_y+t_totalRadius;
	Array<c_ClipVertex* > t_clipPoints1=m_s_clipPoints1;
	Array<c_ClipVertex* > t_clipPoints2=m_s_clipPoints2;
	int t_np=0;
	t_np=m_ClipSegmentToLine(t_clipPoints1,t_incidentEdge,t_tangent2,t_sideOffset1);
	if(t_np<2){
		return;
	}
	t_np=m_ClipSegmentToLine(t_clipPoints2,t_clipPoints1,t_tangent,t_sideOffset2);
	if(t_np<2){
		return;
	}
	t_manifold->m_m_localPlaneNormal->p_SetV(t_localNormal);
	t_manifold->m_m_localPoint->p_SetV(t_planePoint);
	int t_pointCount=0;
	for(int t_i=0;t_i<2;t_i=t_i+1){
		t_cv=t_clipPoints2[t_i];
		Float t_separation=t_normal->m_x*t_cv->m_v->m_x+t_normal->m_y*t_cv->m_v->m_y-t_frontOffset;
		if(t_separation<=t_totalRadius){
			c_b2ManifoldPoint* t_cp=t_manifold->m_m_points[t_pointCount];
			t_tMat=t_xf2->m_R;
			Float t_tX=t_cv->m_v->m_x-t_xf2->m_position->m_x;
			Float t_tY=t_cv->m_v->m_y-t_xf2->m_position->m_y;
			t_cp->m_m_localPoint->m_x=t_tX*t_tMat->m_col1->m_x+t_tY*t_tMat->m_col1->m_y;
			t_cp->m_m_localPoint->m_y=t_tX*t_tMat->m_col2->m_x+t_tY*t_tMat->m_col2->m_y;
			t_cp->m_m_id->p_Set8(t_cv->m_id);
			t_cp->m_m_id->m_features->p_Flip2(t_flip);
			t_pointCount+=1;
		}
	}
	t_manifold->m_m_pointCount=t_pointCount;
}
void c_b2Collision::mark(){
	Object::mark();
}
c_b2CircleContact::c_b2CircleContact(){
}
c_b2CircleContact* c_b2CircleContact::m_new(){
	c_b2Contact::m_new();
	return this;
}
void c_b2CircleContact::p_Reset(c_b2Fixture* t_fixtureA,c_b2Fixture* t_fixtureB){
	c_b2Contact::p_Reset(t_fixtureA,t_fixtureB);
}
void c_b2CircleContact::p_Evaluate(){
	c_b2Body* t_bA=m_m_fixtureA->p_GetBody();
	c_b2Body* t_bB=m_m_fixtureB->p_GetBody();
	c_b2Collision::m_CollideCircles(m_m_manifold,dynamic_cast<c_b2CircleShape*>(m_m_fixtureA->p_GetShape()),t_bA->m_m_xf,dynamic_cast<c_b2CircleShape*>(m_m_fixtureB->p_GetShape()),t_bB->m_m_xf);
}
void c_b2CircleContact::mark(){
	c_b2Contact::mark();
}
c_b2PolygonContact::c_b2PolygonContact(){
}
c_b2PolygonContact* c_b2PolygonContact::m_new(){
	c_b2Contact::m_new();
	return this;
}
void c_b2PolygonContact::p_Reset(c_b2Fixture* t_fixtureA,c_b2Fixture* t_fixtureB){
	c_b2Contact::p_Reset(t_fixtureA,t_fixtureB);
}
void c_b2PolygonContact::p_Evaluate(){
	c_b2Body* t_bA=m_m_fixtureA->p_GetBody();
	c_b2Body* t_bB=m_m_fixtureB->p_GetBody();
	c_b2Collision::m_CollidePolygons(m_m_manifold,dynamic_cast<c_b2PolygonShape*>(m_m_fixtureA->p_GetShape()),t_bA->m_m_xf,dynamic_cast<c_b2PolygonShape*>(m_m_fixtureB->p_GetShape()),t_bB->m_m_xf);
}
void c_b2PolygonContact::mark(){
	c_b2Contact::mark();
}
c_b2EdgeAndCircleContact::c_b2EdgeAndCircleContact(){
}
c_b2EdgeAndCircleContact* c_b2EdgeAndCircleContact::m_new(){
	c_b2Contact::m_new();
	return this;
}
void c_b2EdgeAndCircleContact::p_Reset(c_b2Fixture* t_fixtureA,c_b2Fixture* t_fixtureB){
	c_b2Contact::p_Reset(t_fixtureA,t_fixtureB);
}
void c_b2EdgeAndCircleContact::p_B2CollideEdgeAndCircle(c_b2Manifold* t_manifold,c_b2EdgeShape* t_edge,c_b2Transform* t_xf1,c_b2CircleShape* t_circle,c_b2Transform* t_xf2){
}
void c_b2EdgeAndCircleContact::p_Evaluate(){
	c_b2Body* t_bA=m_m_fixtureA->p_GetBody();
	c_b2Body* t_bB=m_m_fixtureB->p_GetBody();
	p_B2CollideEdgeAndCircle(m_m_manifold,dynamic_cast<c_b2EdgeShape*>(m_m_fixtureA->p_GetShape()),t_bA->m_m_xf,dynamic_cast<c_b2CircleShape*>(m_m_fixtureB->p_GetShape()),t_bB->m_m_xf);
}
void c_b2EdgeAndCircleContact::mark(){
	c_b2Contact::mark();
}
c_b2PolyAndEdgeContact::c_b2PolyAndEdgeContact(){
}
c_b2PolyAndEdgeContact* c_b2PolyAndEdgeContact::m_new(){
	c_b2Contact::m_new();
	return this;
}
void c_b2PolyAndEdgeContact::p_Reset(c_b2Fixture* t_fixtureA,c_b2Fixture* t_fixtureB){
	c_b2Contact::p_Reset(t_fixtureA,t_fixtureB);
}
void c_b2PolyAndEdgeContact::p_B2CollidePolyAndEdge(c_b2Manifold* t_manifold,c_b2PolygonShape* t_polygon,c_b2Transform* t_xf1,c_b2EdgeShape* t_edge,c_b2Transform* t_xf2){
}
void c_b2PolyAndEdgeContact::p_Evaluate(){
	c_b2Body* t_bA=m_m_fixtureA->p_GetBody();
	c_b2Body* t_bB=m_m_fixtureB->p_GetBody();
	p_B2CollidePolyAndEdge(m_m_manifold,dynamic_cast<c_b2PolygonShape*>(m_m_fixtureA->p_GetShape()),t_bA->m_m_xf,dynamic_cast<c_b2EdgeShape*>(m_m_fixtureB->p_GetShape()),t_bB->m_m_xf);
}
void c_b2PolyAndEdgeContact::mark(){
	c_b2Contact::mark();
}
c_ClipVertex::c_ClipVertex(){
	m_v=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	m_id=(new c_b2ContactID)->m_new();
}
c_ClipVertex* c_ClipVertex::m_new(){
	return this;
}
void c_ClipVertex::p_Set14(c_ClipVertex* t_other){
	m_v->m_x=t_other->m_v->m_x;
	m_v->m_y=t_other->m_v->m_y;
	m_id->p_Set8(t_other->m_id);
}
void c_ClipVertex::mark(){
	Object::mark();
	gc_mark_q(m_v);
	gc_mark_q(m_id);
}
int bbInit(){
	GC_CTOR
	bb_app__app=0;
	bb_app__delegate=0;
	bb_app__game=BBGame::Game();
	bb_graphics_device=0;
	bb_graphics_context=(new c_GraphicsContext)->m_new();
	c_Image::m_DefaultFlags=0;
	bb_audio_device=0;
	bb_input_device=0;
	bb_app__devWidth=0;
	bb_app__devHeight=0;
	bb_app__displayModes=Array<c_DisplayMode* >();
	bb_app__desktopMode=0;
	bb_graphics_renderDevice=0;
	c_VirtualDisplay::m_Display=0;
	c_b2World::m_m_warmStarting=false;
	c_b2World::m_m_continuousPhysics=false;
	c_b2ContactFilter::m_b2_defaultFilter=(new c_b2ContactFilter)->m_new();
	c_b2ContactListener::m_b2_defaultListener=((new c_b2ContactListener)->m_new());
	c_b2DebugDraw::m_e_shapeBit=1;
	c_b2DebugDraw::m_e_jointBit=2;
	bb_Rebound_currentVersionCode=105;
	bb_Rebound_versionCode=0;
	bb_app__updateRate=0;
	c_b2Fixture::m_tmpAABB1=(new c_b2AABB)->m_new();
	c_b2Fixture::m_tmpAABB2=(new c_b2AABB)->m_new();
	c_b2Fixture::m_tmpVec=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	c_Game::m_sensorColliding=false;
	c_b2World::m_s_timestep2=(new c_b2TimeStep)->m_new();
	c_b2Distance::m_b2_gjkCalls=0;
	c_b2Distance::m_s_simplex=(new c_b2Simplex)->m_new();
	c_b2Simplex::m_tmpVec1=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	c_b2Simplex::m_tmpVec2=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	c_b2Distance::m_s_saveA=Array<int >(3);
	c_b2Distance::m_s_saveB=Array<int >(3);
	c_b2Distance::m_tmpVec1=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	c_b2Simplex::m_tmpVec3=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	c_b2Distance::m_tmpVec2=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	c_b2Distance::m_b2_gjkIters=0;
	c_b2Distance::m_b2_gjkMaxIters=0;
	c_b2ContactSolver::m_s_worldManifold=(new c_b2WorldManifold)->m_new();
	c_b2ContactSolver::m_s_psm=(new c_b2PositionSolverManifold)->m_new();
	c_b2Island::m_s_impulse=(new c_b2ContactImpulse)->m_new();
	c_b2Body::m_s_xf1=(new c_b2Transform)->m_new(0,0);
	c_b2World::m_s_queue=Array<c_b2Body* >(256);
	c_b2Contact::m_s_input=(new c_b2TOIInput)->m_new();
	c_b2TimeOfImpact::m_b2_toiCalls=0;
	c_b2TimeOfImpact::m_s_cache=(new c_b2SimplexCache)->m_new();
	c_b2TimeOfImpact::m_s_distanceInput=(new c_b2DistanceInput)->m_new();
	c_b2TimeOfImpact::m_s_xfA=(new c_b2Transform)->m_new(0,0);
	c_b2TimeOfImpact::m_s_xfB=(new c_b2Transform)->m_new(0,0);
	c_b2TimeOfImpact::m_s_distanceOutput=(new c_b2DistanceOutput)->m_new();
	c_b2TimeOfImpact::m_s_fcn=(new c_b2SeparationFunction)->m_new();
	c_b2SeparationFunction::m_tmpTransA=(new c_b2Transform)->m_new(0,0);
	c_b2SeparationFunction::m_tmpTransB=(new c_b2Transform)->m_new(0,0);
	c_b2SeparationFunction::m_tmpVec1=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	c_b2SeparationFunction::m_tmpVec2=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	c_b2SeparationFunction::m_tmpVec3=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	c_b2TimeOfImpact::m_b2_toiRootIters=0;
	c_b2TimeOfImpact::m_b2_toiMaxRootIters=0;
	c_b2TimeOfImpact::m_b2_toiIters=0;
	c_b2TimeOfImpact::m_b2_toiMaxIters=0;
	c_b2World::m_s_backupA=(new c_b2Sweep)->m_new();
	c_b2World::m_s_backupB=(new c_b2Sweep)->m_new();
	c_b2World::m_s_timestep=(new c_b2TimeStep)->m_new();
	c_b2World::m_s_jointColor=(new c_b2Color)->m_new(FLOAT(0.5),FLOAT(0.8),FLOAT(0.8));
	c_b2DebugDraw::m_e_controllerBit=32;
	c_b2DebugDraw::m_e_pairBit=8;
	c_b2DebugDraw::m_e_aabbBit=4;
	c_b2DebugDraw::m_e_centerOfMassBit=16;
	c_b2World::m_s_xf=(new c_b2Transform)->m_new(0,0);
	c_b2DynamicTreeNode::m_idCount=0;
	c_b2DynamicTree::m_shared_aabbCenter=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	c_b2Collision::m_s_edgeAO=Array<int >(1);
	c_b2Collision::m_s_edgeBO=Array<int >(1);
	c_b2Collision::m_s_incidentEdge=c_b2Collision::m_MakeClipPointVector();
	c_b2Collision::m_s_localTangent=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	c_b2Collision::m_s_localNormal=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	c_b2Collision::m_s_planePoint=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	c_b2Collision::m_s_tangent=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	c_b2Collision::m_s_tangent2=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	c_b2Collision::m_s_normal=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	c_b2Collision::m_s_v11=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	c_b2Collision::m_s_v12=(new c_b2Vec2)->m_new(FLOAT(0.0),FLOAT(0.0));
	c_b2Collision::m_s_clipPoints1=c_b2Collision::m_MakeClipPointVector();
	c_b2Collision::m_s_clipPoints2=c_b2Collision::m_MakeClipPointVector();
	return 0;
}
void gc_mark(){
	gc_mark_q(bb_app__app);
	gc_mark_q(bb_app__delegate);
	gc_mark_q(bb_graphics_device);
	gc_mark_q(bb_graphics_context);
	gc_mark_q(bb_audio_device);
	gc_mark_q(bb_input_device);
	gc_mark_q(bb_app__displayModes);
	gc_mark_q(bb_app__desktopMode);
	gc_mark_q(bb_graphics_renderDevice);
	gc_mark_q(c_VirtualDisplay::m_Display);
	gc_mark_q(c_b2ContactFilter::m_b2_defaultFilter);
	gc_mark_q(c_b2ContactListener::m_b2_defaultListener);
	gc_mark_q(c_b2Fixture::m_tmpAABB1);
	gc_mark_q(c_b2Fixture::m_tmpAABB2);
	gc_mark_q(c_b2Fixture::m_tmpVec);
	gc_mark_q(c_b2World::m_s_timestep2);
	gc_mark_q(c_b2Distance::m_s_simplex);
	gc_mark_q(c_b2Simplex::m_tmpVec1);
	gc_mark_q(c_b2Simplex::m_tmpVec2);
	gc_mark_q(c_b2Distance::m_s_saveA);
	gc_mark_q(c_b2Distance::m_s_saveB);
	gc_mark_q(c_b2Distance::m_tmpVec1);
	gc_mark_q(c_b2Simplex::m_tmpVec3);
	gc_mark_q(c_b2Distance::m_tmpVec2);
	gc_mark_q(c_b2ContactSolver::m_s_worldManifold);
	gc_mark_q(c_b2ContactSolver::m_s_psm);
	gc_mark_q(c_b2Island::m_s_impulse);
	gc_mark_q(c_b2Body::m_s_xf1);
	gc_mark_q(c_b2World::m_s_queue);
	gc_mark_q(c_b2Contact::m_s_input);
	gc_mark_q(c_b2TimeOfImpact::m_s_cache);
	gc_mark_q(c_b2TimeOfImpact::m_s_distanceInput);
	gc_mark_q(c_b2TimeOfImpact::m_s_xfA);
	gc_mark_q(c_b2TimeOfImpact::m_s_xfB);
	gc_mark_q(c_b2TimeOfImpact::m_s_distanceOutput);
	gc_mark_q(c_b2TimeOfImpact::m_s_fcn);
	gc_mark_q(c_b2SeparationFunction::m_tmpTransA);
	gc_mark_q(c_b2SeparationFunction::m_tmpTransB);
	gc_mark_q(c_b2SeparationFunction::m_tmpVec1);
	gc_mark_q(c_b2SeparationFunction::m_tmpVec2);
	gc_mark_q(c_b2SeparationFunction::m_tmpVec3);
	gc_mark_q(c_b2World::m_s_backupA);
	gc_mark_q(c_b2World::m_s_backupB);
	gc_mark_q(c_b2World::m_s_timestep);
	gc_mark_q(c_b2World::m_s_jointColor);
	gc_mark_q(c_b2World::m_s_xf);
	gc_mark_q(c_b2DynamicTree::m_shared_aabbCenter);
	gc_mark_q(c_b2Collision::m_s_edgeAO);
	gc_mark_q(c_b2Collision::m_s_edgeBO);
	gc_mark_q(c_b2Collision::m_s_incidentEdge);
	gc_mark_q(c_b2Collision::m_s_localTangent);
	gc_mark_q(c_b2Collision::m_s_localNormal);
	gc_mark_q(c_b2Collision::m_s_planePoint);
	gc_mark_q(c_b2Collision::m_s_tangent);
	gc_mark_q(c_b2Collision::m_s_tangent2);
	gc_mark_q(c_b2Collision::m_s_normal);
	gc_mark_q(c_b2Collision::m_s_v11);
	gc_mark_q(c_b2Collision::m_s_v12);
	gc_mark_q(c_b2Collision::m_s_clipPoints1);
	gc_mark_q(c_b2Collision::m_s_clipPoints2);
}
//${TRANSCODE_END}

int main( int argc,const char *argv[] ){

	BBMonkeyGame::Main( argc,argv );
}
