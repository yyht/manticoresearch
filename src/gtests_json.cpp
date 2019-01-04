//
// Created by alexey on 25.08.17.
//

#include <gtest/gtest.h>

#include "json/cJSON.h"
#include "sphinxjson.h"
#include "sphinxjsonquery.h"

// Miscelaneous short tests for json/cjson

//////////////////////////////////////////////////////////////////////////

TEST ( CJson, basics )
{
	struct MyIndex_t
	{
		CSphString m_sName;
		CSphString m_sPath;
	};

	CSphString sResult;

	CSphVector<MyIndex_t> dIndexes;
	dIndexes.Add ( { "test1", "test1_path" } );
	dIndexes.Add ( { "test2", "test2_path" } );
	dIndexes.Add ( { "test3", "test3_path" } );

	{
		cJSON * pRoot = cJSON_CreateObject ();
		ASSERT_TRUE ( pRoot );

		cJSON * pIndexes = cJSON_CreateArray ();
		ASSERT_TRUE ( pIndexes );
		cJSON_AddItemToObject ( pRoot, "indexes", pIndexes );

		for ( auto i : dIndexes )
		{
			cJSON * pIndex = cJSON_CreateObject ();
			ASSERT_TRUE ( pIndex );
			cJSON_AddItemToArray ( pIndexes, pIndex );
			cJSON_AddStringToObject ( pIndex, "name", i.m_sName.cstr () );
			cJSON_AddStringToObject ( pIndex, "path", i.m_sPath.cstr () );
		}

		char * szResult = cJSON_Print ( pRoot );
		sResult.Adopt ( &szResult );
		cJSON_Delete ( pRoot );
	}

	{
		const char * dContents = sResult.cstr ();

		cJSON * pRoot = cJSON_Parse ( dContents );
		EXPECT_TRUE ( pRoot );

		cJSON * pIndexes = cJSON_GetObjectItem ( pRoot, "indexes" );
		EXPECT_TRUE ( pIndexes );

		int iNumIndexes = cJSON_GetArraySize ( pIndexes );
		ASSERT_EQ ( iNumIndexes, dIndexes.GetLength () );

		int iItem = 0;
		for ( auto i : dIndexes )
		{
			cJSON * pIndex = cJSON_GetArrayItem ( pIndexes, iItem++ );
			EXPECT_TRUE ( pIndex );

			cJSON * pJ;
			pJ = cJSON_GetObjectItem ( pIndex, "name" );
			EXPECT_TRUE ( pJ );
			ASSERT_EQ ( i.m_sName, pJ->valuestring );

			pJ = cJSON_GetObjectItem ( pIndex, "path" );
			EXPECT_TRUE ( pJ );
			ASSERT_EQ ( i.m_sPath, pJ->valuestring );
		}
		cJSON_Delete ( pRoot );
	}
}

TEST ( CJson, format )
{
	cJSON * pJson = cJSON_CreateObject ();
	cJSON_AddStringToObject ( pJson, "escaped", " \" quote \\ slash \b b \f feed \n n \r r \t tab \005 / here " );
	char * szResult = cJSON_PrintUnformatted ( pJson );
	CSphString sResult ( szResult );
	printf ( "\n%s\n", szResult );
	SafeDeleteArray ( szResult );
	JsonEscapedBuilder tBuild;
	tBuild.StartBlock (":", "{", "}");
	tBuild.AppendString ("escaped", '\"');
	tBuild.AppendEscaped ( " \" quote \\ slash \b b \f feed \n n \r r \t tab \005 / here ", EscBld::eEscape );
	tBuild.FinishBlocks ();
	printf ( "\n%s\n", tBuild.cstr() );

}

// defined in sphinxjson
int sphJsonUnescape ( char ** pEscaped, int iLen );
int sphJsonUnescape1 ( char ** pEscaped, int iLen );

// defined in cJSON_test
extern "C"
{
int cJsonunescape ( char ** buf, cJSON * pOut );
}

TEST ( bench, DISABLED_json_unescape )
{
	auto uLoops = 1000000;
	cJSON * pJson = cJSON_CreateObject ();

	const char sLiteral[] = R"("In `docs/searching/expressions,_functions,_and_operators.rst` which reflected into\\nhttps://manticoresearch.gitlab.io/dev/searching/expressions,_functions,_and_operators.html\\n\\n1. At the top there is a kind of TOC with shortcuts to the functions described in the section.\\nHowever this TOC is not consistent. I.e., it doesn't refer to all function actually described there.\\n\\nMost prominent example is 'PACKEDFACTORS()' - it absent in the TOC.\\n\\n2. Also consider whether it is better or not to sort function descriptions in the section alphabetically ('REMAP' at the end looks strange, as 'WEIGHT' is before it).")";
	auto iLen = strlen ( sLiteral );
	char buf[sizeof(sLiteral)];

	auto iTimeSpan = -sphMicroTimer ();
	char * sBuf = nullptr;
	int iRes = 0;
	for ( auto i = 0; i<uLoops; ++i )
	{
		memcpy ( buf, sLiteral, iLen );
		sBuf = buf;
		iRes = sphJsonUnescape ( &sBuf, iLen );
	}
	iTimeSpan += sphMicroTimer ();
	std::cout << "\n" << uLoops << " of sphJsonUnescape took " << iTimeSpan << " uSec";
	sBuf[iRes] = '\0';
	std::cout << "\n" << iRes << " bytes: " << sBuf << "\n";

	iTimeSpan = -sphMicroTimer ();
	for ( auto i = 0; i<uLoops; ++i )
	{
		memcpy ( buf, sLiteral, iLen );
		sBuf = buf;
		iRes = sphJsonUnescape1 ( &sBuf, iLen );
	}
	iTimeSpan += sphMicroTimer ();
	std::cout << "\n" << uLoops << " of sphJsonUnescape1 took " << iTimeSpan << " uSec";
	sBuf[iRes] = '\0';
	std::cout << "\n" << iRes << " bytes: " << sBuf << "\n";

	iTimeSpan = -sphMicroTimer ();
	for ( auto i = 0; i<uLoops; ++i )
	{
		memcpy ( buf, sLiteral, iLen );
		sBuf = buf;
		iRes = cJsonunescape ( &sBuf, pJson );
	}
	iTimeSpan += sphMicroTimer ();
	std::cout << "\n" << uLoops << " of cJsonunescape took " << iTimeSpan << " uSec";

	iTimeSpan = -sphMicroTimer ();
	for ( auto i = 0; i<uLoops; ++i )
	{
		memcpy ( buf, sLiteral, iLen );
		sBuf = buf;
	}
	iTimeSpan += sphMicroTimer ();
	std::cout << "\n" << uLoops << " of payload memcpy took " << iTimeSpan << " uSec";
}

using namespace bson;

class TJson : public ::testing::Test
{

protected:
	virtual void SetUp ()
	{
		dData.Reset();
		sError = "";
	}
	CSphVector<BYTE> dData;
	CSphString sError;


	bool testcase ( const char * sJson, bool bAutoconv = false, bool bToLowercase = false )
	{
		CSphString sText = sJson;
		return sphJsonParse ( dData, ( char * ) sText.cstr (), bAutoconv, bToLowercase, sError );
	}

	// helper: parse given str into internal bson
	NodeHandle_t Bson ( const char * sJson )
	{
		CSphString sText = sJson;
		CSphString sError;
		dData.Reset ();
		sphJsonParse ( dData, ( char * ) sText.cstr(), false, true, sError );
		if ( dData.IsEmpty () )
			return nullnode;

		NodeHandle_t sResult;
		const BYTE * pData = dData.begin ();
		sResult.second = sphJsonFindFirst ( &pData );
		sResult.first = pData;
		return sResult;
	}

	// helper : parse given str into internal bson and split it to variables
	CSphVector<Bson_c> Bsons ( const char * sJson)
	{
		Bson_c dRoot ( Bson ( sJson ));
		CSphVector<Bson_c> dResult;
		dRoot.ForEach([&](const NodeHandle_t& dNode){
			dResult.Add (dNode);
		});
		return dResult;
	}
};



TEST_F( TJson, parser )
{
	ASSERT_TRUE ( testcase ( R"({sv:["one","two","three"],sp:["foo","fee"],gid:315})" ) );
	//                          0    5    11    17       26  30     36     43  47
	ASSERT_TRUE ( testcase ( "[]", true, false ) );
	ASSERT_TRUE ( testcase ( R"({"name":"Alice","uid":123})" ) );
	ASSERT_TRUE ( testcase ( R"({key1:{key2:{key3:"value"}}})" ) );
	ASSERT_TRUE ( testcase ( R"([6,[6,[6,[6,6.0]]]])" ) );

	ASSERT_TRUE ( testcase ( R"({"name":"Bob","uid":234,"gid":12})" ) );
	ASSERT_TRUE ( testcase ( R"({"name":"Charlie","uid":345})" ) );
	ASSERT_TRUE ( testcase ( R"({"12":345, "34":"567"})", true ) );
	ASSERT_TRUE ( testcase ( R"({
	i1:"123",
	i2:"-123",
	i3:"18446744073709551615",
	i4:"-18446744073709551615",
	i5:"9223372036854775807",
	i6:"9223372036854775808",
	i7:"9223372036854775809",
	i8:"-9223372036854775807",
	i9:"-9223372036854775808",
	i10:"-9223372036854775809",
	i11:"123abc",
	i12:"-123abc",
	f1:"3.15",
	f2:"16777217.123"})", true ) );
	ASSERT_TRUE ( testcase ( R"({
	i11:"123abc",
	i12:"-123abc",
	f1:"3.15",
	f2:"16777217.123"})", true ) );

}

TEST_F ( TJson, accessor )
{

	BsonContainer_c dBson (
		R"({ "query": { "percolate": { "document" : { "title" : "A new tree test in the office office" } } } })");
	auto dDocs = dBson.ChildByPath ("query.percolate.document");

//	auto pMember = dBson->ChildByPath ( "query.percolate.document" );
//	auto pMembers = dBson->ChildByPath ( "query.percolate.documents" );
//	auto dQuery = (*dBson)["percolate"];
//	auto dDoc = (*dQuery)["documeht"];
	auto dTitle = Bson_c ( dDocs ).ChildByPath ( "title" );
	auto dTitle1 = Bson_c ( dDocs ).ChildByName ( "title" );

	ASSERT_TRUE (true);
}

// test bson::Bool
TEST_F ( TJson, bson_Bool )
{
	auto tst = Bsons ("[12345678, 0, 123456789000000, 1.0, 0.0, true, false, \"abc\", {}, []]");

	ASSERT_TRUE ( tst[0].Bool () );
	ASSERT_FALSE ( tst[1].Bool () );
	ASSERT_TRUE ( tst[2].Bool () );
	ASSERT_TRUE ( tst[3].Bool () );
	ASSERT_FALSE ( tst[4].Bool () );
	ASSERT_TRUE ( tst[5].Bool () );
	ASSERT_FALSE ( tst[6].Bool () );
	ASSERT_FALSE ( tst[7].Bool () );
	ASSERT_FALSE ( tst[8].Bool () );
	ASSERT_FALSE ( tst[9].Bool () );
}

// test bson::Int
TEST_F ( TJson, bson_Int )
{
	auto tst = Bsons ( R"([12345678, 123456789000000, 1.0, true, false, "123","1.13","123abc", {}, []])" );

	ASSERT_EQ ( tst[0].Int (), 12345678 );
	ASSERT_EQ ( tst[1].Int (), 123456789000000 );
	ASSERT_EQ ( tst[2].Int (), 1 );
	ASSERT_EQ ( tst[3].Int (), 1 );
	ASSERT_EQ ( tst[4].Int (), 0 );
	ASSERT_EQ ( tst[5].Int (), 123 );
	ASSERT_EQ ( tst[6].Int (), 1 );
	ASSERT_EQ ( tst[7].Int (), 0 );
	ASSERT_EQ ( tst[8].Int (), 0 );
	ASSERT_EQ ( tst[9].Int (), 0 );
}

// test bson::Double
TEST_F ( TJson, bson_Double )
{
	auto tst = Bsons ( R"([12345678, 123456789000000, 1.23, true, false, "123","1.13","123abc", {}, []])" );

	ASSERT_EQ ( tst[0].Double (), 12345678.0 );
	ASSERT_EQ ( tst[1].Double (), 123456789000000.0 );
	ASSERT_EQ ( tst[2].Double (), 1.23 );
	ASSERT_EQ ( tst[3].Double (), 1.0 );
	ASSERT_EQ ( tst[4].Double (), 0.0 );
	ASSERT_EQ ( tst[5].Double (), 123.0 );
	ASSERT_EQ ( tst[6].Double (), 1.13 );
	ASSERT_EQ ( tst[7].Double (), 0.0 );
	ASSERT_EQ ( tst[8].Double (), 0.0 );
	ASSERT_EQ ( tst[8].Double (), 0.0 );
}

// test bson::String
TEST_F ( TJson, bson_String )
{
	auto tst = Bsons ( R"([12345678, 123456789000000, 1.23, true, false, "123","1.13","123abc", {}, []])" );

	ASSERT_STREQ ( tst[0].String ().cstr (), "" );
	ASSERT_STREQ ( tst[1].String ().cstr (), "" );
	ASSERT_STREQ ( tst[2].String ().cstr (), "" );
	ASSERT_STREQ ( tst[3].String ().cstr (), "" );
	ASSERT_STREQ ( tst[4].String ().cstr (), "" );
	ASSERT_STREQ ( tst[5].String ().cstr (), "123" );
	ASSERT_STREQ ( tst[6].String ().cstr (), "1.13" );
	ASSERT_STREQ ( tst[7].String ().cstr (), "123abc" );
	ASSERT_STREQ ( tst[8].String ().cstr (), "" );
	ASSERT_STREQ ( tst[8].String ().cstr (), "" );
}

// "foreach" over the vec
TEST_F ( TJson, bson_foreach_vec )
{
	Bson_c tst = Bson ( R"([12345678, 123456789000000, 1.23, true, false, "123","1.13","123abc", {}, []])" );
	ESphJsonType dTypes[] = {JSON_INT32,JSON_INT64,JSON_DOUBLE,JSON_TRUE,JSON_FALSE,JSON_STRING,
						  JSON_STRING,JSON_STRING,JSON_OBJECT,JSON_MIXED_VECTOR};
	int iIdx = 0;
	tst.ForEach ([&](const NodeHandle_t& dNode){
		ASSERT_EQ ( dNode.second, dTypes[iIdx++] );
	});

	iIdx = 0;
	tst.ForEach ( [&] ( Bson_c dNode ) {
		ASSERT_EQ ( dNode.GetType (), dTypes[iIdx++] );
	} );
}

// named "foreach" over the vec
TEST_F ( TJson, bson_foreach_namedvec )
{
	Bson_c tst = Bson ( R"([12345678, 123456789000000, 1.23, true, false, "123","1.13","123abc", {}, []])" );
	ESphJsonType dTypes[] = { JSON_INT32, JSON_INT64, JSON_DOUBLE, JSON_TRUE, JSON_FALSE, JSON_STRING, JSON_STRING
							  , JSON_STRING, JSON_OBJECT, JSON_MIXED_VECTOR };
	int iIdx = 0;
	tst.ForEach ( [&] ( CSphString sName, const NodeHandle_t &dNode ) {
		ASSERT_STREQ (sName.cstr(),"");
		ASSERT_EQ ( dNode.second, dTypes[iIdx++] );
	} );

	iIdx = 0;
	tst.ForEach ( [&] ( CSphString sName, Bson_c dNode ) {
		ASSERT_STREQ ( sName.cstr (), "" );
		ASSERT_EQ ( dNode.GetType (), dTypes[iIdx++] );
	} );
}

// "foreach" over obj
TEST_F ( TJson, bson_foreach_obj )
{
	Bson_c tst = Bson ( R"({a:12345678, b:123456789000000, c:1.23, d:true, e:false, f:"123",g:"1.13",H:"123abc", i:{}, j:[]])");
	ESphJsonType dTypes[] = { JSON_INT32, JSON_INT64, JSON_DOUBLE, JSON_TRUE, JSON_FALSE, JSON_STRING, JSON_STRING
							  , JSON_STRING, JSON_OBJECT, JSON_MIXED_VECTOR };
	int iIdx = 0;
	tst.ForEach ( [&] ( const NodeHandle_t &dNode ) {
		ASSERT_EQ ( dNode.second, dTypes[iIdx++] );
	} );

	iIdx = 0;
	tst.ForEach ( [&] ( Bson_c dNode ) {
		ASSERT_EQ ( dNode.GetType (), dTypes[iIdx++] );
	} );
}

// named "foreach" over obj
TEST_F ( TJson, bson_foreach_namedobj )
{
	Bson_c tst = Bson (
		R"({a:12345678, b:123456789000000, c:1.23, d:true, e:false, f:"123",g:"1.13",H:"123abc", i:{}, j:[]])" );
	ESphJsonType dTypes[] = { JSON_INT32, JSON_INT64, JSON_DOUBLE, JSON_TRUE, JSON_FALSE, JSON_STRING, JSON_STRING
							  , JSON_STRING, JSON_OBJECT, JSON_MIXED_VECTOR };
	const char* sNames[] = {"a","b","c","d","e","f","g",
						 "h", // note that name is lowercase in opposite 'H' in the object
						 "i","j"};
	int iIdx = 0;
	tst.ForEach ( [&] ( CSphString sName, const NodeHandle_t &dNode ) {
		ASSERT_STREQ ( sName.cstr (), sNames[iIdx] );
		ASSERT_EQ ( dNode.second, dTypes[iIdx++] );
	} );

	iIdx = 0;
	tst.ForEach ( [&] ( CSphString sName, Bson_c dNode ) {
		ASSERT_STREQ ( sName.cstr (), sNames[iIdx] );
		ASSERT_EQ ( dNode.GetType (), dTypes[iIdx++] );
	} );
}

void assert_eq( ESphJsonType a, ESphJsonType b )
{
	ASSERT_EQ (a, b);
}

// "foreach" over the vec
TEST_F ( TJson, bson_forsome_vec )
{
	Bson_c tst = Bson ( R"([12345678, 123456789000000, 1.23, true, false, "123","1.13","123abc", {}, []])" );
	ESphJsonType dTypes[] = { JSON_INT32, JSON_INT64, JSON_DOUBLE, JSON_TRUE, JSON_FALSE, JSON_STRING, JSON_STRING
							  , JSON_STRING, JSON_OBJECT, JSON_MIXED_VECTOR };
	int iIdx = 0;
	tst.ForSome ( [&] ( const NodeHandle_t &tNode ) {
		// this strange lambda here is need because ASSERT_EQ macro confuses outside lambda's deduction
		[&] () { ASSERT_EQ ( tNode.second, dTypes[iIdx++] ); } ();
		return iIdx<4;
	} );
	ASSERT_EQ (iIdx,4);

	iIdx = 0;
	tst.ForSome ( [&] ( Bson_c dNode ) {
		[&] () { ASSERT_EQ ( dNode.GetType (), dTypes[iIdx++] ); } ();
		return iIdx<4;
	} );
	ASSERT_EQ ( iIdx, 4 );
}

// named "foreach" over the vec
TEST_F ( TJson, bson_forsome_namedvec )
{
	Bson_c tst = Bson ( R"([12345678, 123456789000000, 1.23, true, false, "123","1.13","123abc", {}, []])" );
	ESphJsonType dTypes[] = { JSON_INT32, JSON_INT64, JSON_DOUBLE, JSON_TRUE, JSON_FALSE, JSON_STRING, JSON_STRING
							  , JSON_STRING, JSON_OBJECT, JSON_MIXED_VECTOR };
	int iIdx = 0;
	tst.ForEach ( [&] ( CSphString sName, const NodeHandle_t &dNode ) {
		ASSERT_STREQ ( sName.cstr (), "" );
		ASSERT_EQ ( dNode.second, dTypes[iIdx++] );
	} );

	iIdx = 0;
	tst.ForEach ( [&] ( CSphString sName, Bson_c dNode ) {
		ASSERT_STREQ ( sName.cstr (), "" );
		ASSERT_EQ ( dNode.GetType (), dTypes[iIdx++] );
	} );
}

// "foreach" over obj
TEST_F ( TJson, bson_forsome_obj )
{
	Bson_c tst = Bson (
		R"({a:12345678, b:123456789000000, c:1.23, d:true, e:false, f:"123",g:"1.13",H:"123abc", i:{}, j:[]])" );
	ESphJsonType dTypes[] = { JSON_INT32, JSON_INT64, JSON_DOUBLE, JSON_TRUE, JSON_FALSE, JSON_STRING, JSON_STRING
							  , JSON_STRING, JSON_OBJECT, JSON_MIXED_VECTOR };
	int iIdx = 0;
	tst.ForEach ( [&] ( const NodeHandle_t &dNode ) {
		ASSERT_EQ ( dNode.second, dTypes[iIdx++] );
	} );

	iIdx = 0;
	tst.ForEach ( [&] ( Bson_c dNode ) {
		ASSERT_EQ ( dNode.GetType (), dTypes[iIdx++] );
	} );
}

// named "foreach" over obj
TEST_F ( TJson, bson_forsome_namedobj )
{
	Bson_c tst = Bson (
		R"({a:12345678, b:123456789000000, c:1.23, d:true, e:false, f:"123",g:"1.13",H:"123abc", i:{}, j:[]])" );
	ESphJsonType dTypes[] = { JSON_INT32, JSON_INT64, JSON_DOUBLE, JSON_TRUE, JSON_FALSE, JSON_STRING, JSON_STRING
							  , JSON_STRING, JSON_OBJECT, JSON_MIXED_VECTOR };
	const char * sNames[] = { "a", "b", "c", "d", "e", "f", "g", "h"
							  , // note that name is lowercase in opposite 'H' in the object
		"i", "j" };
	int iIdx = 0;
	tst.ForEach ( [&] ( CSphString sName, const NodeHandle_t &dNode ) {
		ASSERT_STREQ ( sName.cstr (), sNames[iIdx] );
		ASSERT_EQ ( dNode.second, dTypes[iIdx++] );
	} );

	iIdx = 0;
	tst.ForEach ( [&] ( CSphString sName, Bson_c dNode ) {
		ASSERT_STREQ ( sName.cstr (), sNames[iIdx] );
		ASSERT_EQ ( dNode.GetType (), dTypes[iIdx++] );
	} );
}

TEST_F ( TJson, bson_rawblob )
{
	// blob of ints
	Bson_c tst = Bson (	"[0,1,2,3,4]" );
	auto dBlob = bson::RawBlob ( tst );
	ASSERT_EQ ( dBlob.second, 5 );
	auto pValues = (int*)dBlob.first;
	for (int i=0; i<4; ++i)
		ASSERT_EQ ( pValues[i], i);

	// blob of mixed (must not work)
	tst = Bson ( "[0,1,2,300000000000000,4]" );
	dBlob = bson::RawBlob ( tst );
	ASSERT_EQ ( dBlob.second, 0 ); // since values are different, Bson is mixed vector, which can't be blob

	// blob of int64
	tst = Bson ( "[100000000000,100000000001,100000000002,100000000003,100000000004]" );
	dBlob = bson::RawBlob ( tst );
	ASSERT_EQ ( dBlob.second, 5 );
	auto pValues64 = ( int64_t * ) dBlob.first;
	ASSERT_EQ ( pValues64[0], 100000000000);
	ASSERT_EQ ( pValues64[1], 100000000001 );
	ASSERT_EQ ( pValues64[2], 100000000002 );
	ASSERT_EQ ( pValues64[3], 100000000003 );
	ASSERT_EQ ( pValues64[4], 100000000004 );

	// blob of doubles
	tst = Bson ( "[0.0,0.1,0.2,0.3,0.4]" );
	dBlob = bson::RawBlob ( tst );
	ASSERT_EQ ( dBlob.second, 5 );
	auto pValuesD = ( double * ) dBlob.first;
	ASSERT_EQ ( pValuesD[0], 0.0 );
	ASSERT_EQ ( pValuesD[1], 0.1 );
	ASSERT_EQ ( pValuesD[2], 0.2 );
	ASSERT_EQ ( pValuesD[3], 0.3 );
	ASSERT_EQ ( pValuesD[4], 0.4 );

	// string is also may be traited as blob
	tst = Bson_c(Bson( "[\"Hello world!\"]" )).ChildByIndex (0);
	dBlob = bson::RawBlob ( tst );
	ASSERT_EQ ( dBlob.second, strlen("Hello world!") );
	ASSERT_EQ ( 0, memcmp (dBlob.first, "Hello world!", strlen("Hello world!")));
}

// test property "IsEmpty"
TEST_F ( TJson, bson_IsEmpty )
{
	ASSERT_TRUE ( Bson_c ( Bson ( "" ) ).IsEmpty () );
	ASSERT_TRUE ( Bson_c ( Bson ( "[]" ) ).IsEmpty () );
	ASSERT_TRUE ( Bson_c ( Bson ( "{}" ) ).IsEmpty () );

	ASSERT_FALSE ( Bson_c ( Bson ( "{a:2}" ) ).IsEmpty () );
	ASSERT_FALSE ( Bson_c ( Bson ( R"(["a","b"])" ) ).IsEmpty () );
	ASSERT_FALSE ( Bson_c ( Bson ( R"(["a","b",3])" ) ).IsEmpty () );
	ASSERT_FALSE ( Bson_c ( Bson ( R"([1])" ) ).IsEmpty () );
}

// test counting of values
TEST_F ( TJson, bson_CountValues )
{
	ASSERT_EQ ( Bson_c ( Bson ( "" ) ).CountValues (), 0 );
	ASSERT_EQ ( Bson_c ( Bson ( "{}" ) ).CountValues (), 0 );

	auto tst = Bsons ( R"([1,1.0,["a","b"],[1,"a"],[1,2],[1.0,2.0],{a:1,b:2,c:3}, {}, [], true, false])" );

	ASSERT_EQ ( tst[0].CountValues (), 1 );
	ASSERT_EQ ( tst[1].CountValues (), 1 );
	ASSERT_EQ ( tst[2].CountValues (), 2 );
	ASSERT_EQ ( tst[3].CountValues (), 2 );
	ASSERT_EQ ( tst[4].CountValues (), 2 );
	ASSERT_EQ ( tst[5].CountValues (), 2 );
	ASSERT_EQ ( tst[6].CountValues (), 3 );
	ASSERT_EQ ( tst[7].CountValues (), 0 );
	ASSERT_EQ ( tst[8].CountValues (), 0 );
	ASSERT_EQ ( tst[9].CountValues (), 0 );
	ASSERT_EQ ( tst[10].CountValues (), 0 );

}

// test str comparision
TEST_F ( TJson, bson_StrEq )
{
	auto tst = Bsons ( R"(["hello","World!"])" );

	ASSERT_TRUE ( tst[0].StrEq ( "hello" ) );
	ASSERT_FALSE ( tst[0].StrEq ( "Hello" ) );
	ASSERT_TRUE ( tst[1].StrEq ( "World!" ) );
	ASSERT_FALSE ( tst[1].StrEq ( "world!" ) );
	ASSERT_FALSE ( tst[1].StrEq ( "world" ) );
}

// test access direct children of assocs by name
TEST_F ( TJson, bson_child_by_name )
{
	Bson_c tst = Bson ( R"({first :1, Second: 2,"third" :3,"Fourth":4})" );

	ASSERT_EQ ( Bson_c ( tst.ChildByName ( "first" ) ).Int (), 1 );
	ASSERT_EQ ( Bson_c ( tst.ChildByName ( "second" ) ).Int (), 2 );
	ASSERT_EQ ( Bson_c ( tst.ChildByName ( "third" ) ).Int (), 3 );
	ASSERT_EQ ( Bson_c ( tst.ChildByName ( "fourth" ) ).Int (), 4 );

	// no access by index to object members!
	ASSERT_TRUE ( Bson_c ( tst.ChildByIndex ( 0 ) ).IsNull () );
}

// test access to children of array by idx
TEST_F ( TJson, bson_child_by_index )
{
	Bson_c tst = Bson ( R"([1,"abc",2.2])" );

	ASSERT_EQ ( Bson_c ( tst.ChildByIndex ( 0 ) ).Int (), 1 );
	ASSERT_TRUE ( Bson_c ( tst.ChildByIndex ( 1 ) ).StrEq("abc") );
	ASSERT_EQ ( Bson_c ( tst.ChildByIndex ( 2 ) ).Double (), 2.2 );
}

// test access to children of array/obj by complex path
TEST_F ( TJson, bson_child_by_path )
{
	Bson_c tst = Bson ( R"({name:"hello",value:[1,2,{syntax:[1,3,42,13],value:"Here"},"blabla"]})" );

	ASSERT_TRUE ( Bson_c ( tst.ChildByPath ( "name" ) ).StrEq ("hello") );
	ASSERT_EQ ( Bson_c ( tst.ChildByPath ( "value[1]" ) ).Int(), 2 );
	ASSERT_EQ ( Bson_c ( tst.ChildByPath ( "value[2].syntax[2]" ) ).Int (), 42 );
	ASSERT_TRUE ( Bson_c ( tst.ChildByPath ( "value[2].value" ) ).StrEq ( "Here" ) );
	ASSERT_TRUE ( Bson_c ( tst.ChildByPath ( "value[3]" ) ).StrEq ( "blabla" ) );
}

// test HasAnyOf helper
TEST_F ( TJson, bson_has_any_of )
{
	Bson_c tst = Bson ( R"({name:"hello",value1:2,value2:"sdfa",value3:{value4:"foo"}})" );

	ASSERT_TRUE ( tst.HasAnyOf ( 2, "foo", "value3" ) );
	ASSERT_TRUE ( tst.HasAnyOf ( 2, "name", "value1" ) );
	ASSERT_FALSE ( tst.HasAnyOf ( 2, "foo", "bar" ) );
	ASSERT_FALSE ( tst.HasAnyOf ( 2, "foo", "value4" ) );
}

// test bson to json render
TEST_F ( TJson, bson_BsonToJson )
{
	auto tst = Bsons ( R"(["hello",2,3.1415926,{value4:"foo"}])" );

	CSphString sJson;
	tst[0].BsonToJson (sJson);
	ASSERT_STREQ ( sJson.cstr(),"\"hello\"" );

	tst[1].BsonToJson ( sJson );
	ASSERT_STREQ ( sJson.cstr (), "2" );

	tst[2].BsonToJson ( sJson );
	ASSERT_STREQ ( sJson.cstr (), "3.141593" );

	tst[3].BsonToJson ( sJson );
	ASSERT_STREQ ( sJson.cstr (), R"({"value4":"foo"})" );

}

// test contained bson
TEST_F ( TJson, bson_BsonContainer )
{
	BsonContainer_c dBson (
		R"({ "query": { "percolate": { "document" : { "title" : "A new tree test in the office office" } } } })" );

	auto dTitle = Bson_c ( dBson.ChildByPath ( "query.percolate.document.title" ));
	ASSERT_TRUE ( dTitle.StrEq ( "A new tree test in the office office" ));

	CSphString sJson;
	dBson.BsonToJson ( sJson );
	ASSERT_STREQ ( sJson.cstr (), R"({"query":{"percolate":{"document":{"title":"A new tree test in the office office"}}}})" );

}

// test contained bson
TEST_F ( TJson, bson_via_cjson )
{
	const char * sJson = R"({ "query": { "percolate": { "document" : { "title" : "A new tree test in the office office" } } } })";

	auto pCjson = cJSON_Parse ( sJson );

	StringBuilder_c sError;
	CSphVector<BYTE> dBson;
	bson::cJsonToBson (pCjson, dBson, false, sError);

	if ( pCjson )
		cJSON_Delete ( pCjson );

	NodeHandle_t dNode;
	if ( dBson.IsEmpty () )
		return;

	const BYTE * pData = dBson.begin ();
	dNode.second = sphJsonFindFirst ( &pData );
	dNode.first = pData;

	Bson_c dBSON ( dNode );

	auto dTitle = Bson_c ( dBSON.ChildByPath ( "query.percolate.document.title" ) );
	ASSERT_TRUE ( dTitle.StrEq ( "A new tree test in the office office" ) );

	CSphString sNewJson;
	dBSON.BsonToJson ( sNewJson );
	ASSERT_STREQ ( sNewJson.cstr ()
				   , R"({"query":{"percolate":{"document":{"title":"A new tree test in the office office"}}}})" );

}

TEST_F ( TJson, bson_via_cjson_test_consistency )
{
	const char * sJson = R"({ "aR32": [1,2,3,4,20], "ar64": [100000000000,100000000001,100000000002,100000000003,100000000004], "ardbl": [1.1,1.2,1.3], "arrstr":["foo","bar"], "arrmixed":[1,1.0] })";

	auto pCjson = cJSON_Parse ( sJson );

	StringBuilder_c sError;
	CSphVector<BYTE> dBson;
	bson::cJsonToBson ( pCjson, dBson, true, sError );

	if ( pCjson )
		cJSON_Delete ( pCjson );

	NodeHandle_t dNode;
	if ( dBson.IsEmpty () )
		return;

	const BYTE * pData = dBson.begin ();
	dNode.second = sphJsonFindFirst ( &pData );
	dNode.first = pData;

	Bson_c dBSON ( dNode );

	auto d32 = Bson_c ( dBSON.ChildByPath ( "ar32" ));
	auto d64 = Bson_c ( dBSON.ChildByPath ( "ar64" ));
	auto ddbl = Bson_c ( dBSON.ChildByPath ( "ardbl" ));
	auto dmixed = Bson_c ( dBSON.ChildByPath ( "arrmixed" ));

	ASSERT_TRUE ( d32.IsArray () );
	ASSERT_TRUE ( d64.IsArray () );
	ASSERT_TRUE ( ddbl.IsArray () );
	ASSERT_TRUE ( dmixed.IsArray () );
}


TEST ( bench, DISABLED_bson_vs_cjson )
{
	auto uLoops = 1000000;

	const char sLiteral[] = R"({"query":{"percolate":{"document":{"title":"A new tree test in the office office"}}}})";
	auto iLen = strlen ( sLiteral );

	const volatile void * pRes = nullptr;
	CSphString sBuf;

	auto iTimeSpan = -sphMicroTimer ();
	for ( auto i = 0; i<uLoops; ++i )
	{
		sBuf.SetBinary ( sLiteral, iLen );
		auto buf = (char*) sBuf.cstr();
		BsonContainer_c dBson { buf };
		pRes = dBson.ChildByName ( "query" ).first;
	}
	iTimeSpan += sphMicroTimer ();
	std::cout << "\n" << uLoops << " of Bson parse took " << iTimeSpan << " uSec, payload " << (int64_t) pRes;

	iTimeSpan = -sphMicroTimer ();
	for ( auto i = 0; i<uLoops; ++i )
	{
		sBuf.SetBinary ( sLiteral, iLen );
		auto buf = ( char * ) sBuf.cstr ();
		BsonContainer_c dBson { buf, false };
		pRes = dBson.ChildByName ( "query" ).first;
	}
	iTimeSpan += sphMicroTimer ();
	std::cout << "\n" << uLoops << " of Bson parse without lowercase took " << iTimeSpan << " uSec, payload " << ( int64_t ) pRes;


	iTimeSpan = -sphMicroTimer ();
	for ( auto i = 0; i<uLoops; ++i )
	{
		sBuf.SetBinary ( sLiteral, iLen );
		auto buf = ( char * ) sBuf.cstr ();
		auto pBson = cJSON_Parse ( buf );
		CSphString sError;

		pRes = GetJSONPropertyObject ( pBson, "query", sError );
		if ( pBson )
			cJSON_Delete ( pBson );

	}
	iTimeSpan += sphMicroTimer ();
	std::cout << "\n" << uLoops << " of cJson parse took " << iTimeSpan << " uSec, payload " << ( int64_t ) pRes;

	StringBuilder_c sError;
	auto pCjson = cJSON_Parse ( sLiteral );

	for ( auto i = 0; i<uLoops; ++i ) // warmup pass
	{
		CSphVector<BYTE> m_Bson ( iLen );
		bson::cJsonToBson ( pCjson, m_Bson, false, sError );
	}

	iTimeSpan = -sphMicroTimer ();
	for ( auto i = 0; i<uLoops; ++i )
	{
		CSphVector<BYTE> m_Bson ( iLen );
		bson::cJsonToBson ( pCjson, m_Bson, false, sError );
//		pRes = dBson.ChildByName ( "query" ).first;
	}
	iTimeSpan += sphMicroTimer ();
	std::cout << "\n" << uLoops << " of cJsonToBson parse took " << iTimeSpan << " uSec, payload " << ( int64_t ) pRes;

	iTimeSpan = -sphMicroTimer ();
	for ( auto i = 0; i<uLoops; ++i )
	{
		CSphVector<BYTE> m_Bson ( iLen );
		bson::cJsonToBson ( pCjson, m_Bson, true, sError );
//		pRes = dBson.ChildByName ( "query" ).first;
	}
	iTimeSpan += sphMicroTimer ();
	std::cout << "\n" << uLoops << " of cJsonToBson parse with lowercase took " << iTimeSpan << " uSec, payload " << ( int64_t ) pRes;

	if ( pCjson )
		cJSON_Delete ( pCjson );

	iTimeSpan = -sphMicroTimer ();
	for ( auto i = 0; i<uLoops; ++i )
	{
		sBuf.SetBinary ( sLiteral, iLen );
	}
	iTimeSpan += sphMicroTimer ();
	std::cout << "\n" << uLoops << " of payload memcpy took " << iTimeSpan << " uSec";
}