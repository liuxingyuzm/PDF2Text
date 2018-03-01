#include <ApplicationServices/ApplicationServices.h>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

class PDFBoolean;
class PDFNumber;
class PDFString;
class PDFName;
class PDFArray;
class PDFDictionary;
class PDFStream;
class PDFNull;

class PDFObject
{
public:
	virtual ~PDFObject() = default;

	enum type_t
	{
		type_invalid,

		type_boolean,
		type_number,
		type_string,
		type_name,
		type_array,
		type_dict,
		type_stream,
		type_null
	};

	type_t type() const { return _type; }

	bool isBoolean() const { return type() == type_boolean; }
	bool isNumber() const { return type() == type_number; }
	bool isString() const { return type() == type_string; }
	bool isName() const { return type() == type_name; }
	bool isArray() const { return type() == type_array; }
	bool isDictionary() const { return type() == type_dict; }
	bool isStream() const { return type() == type_stream; }
	bool isNull() const { return type() == type_null; }

	const PDFNumber *asNumber() const
	{
		return isNumber() ? (const PDFNumber *)this : nullptr;
	}
	const PDFBoolean *asBoolean() const
	{
		return isBoolean() ? (const PDFBoolean *)this : nullptr;
	}
	const PDFString *asString() const
	{
		return isString() ? (const PDFString *)this : nullptr;
	}
	const PDFName *asName() const
	{
		return isName() ? (const PDFName *)this : nullptr;
	}
	const PDFArray *asArray() const
	{
		return isArray() ? (const PDFArray *)this : nullptr;
	}
	const PDFDictionary *asDictionary() const
	{
		return isDictionary() ? (const PDFDictionary *)this : nullptr;
	}
	const PDFStream *asStream() const
	{
		return isStream() ? (const PDFStream *)this : nullptr;
	}
	const PDFNull *asNull() const
	{
		return isNull() ? (const PDFNull *)this : nullptr;
	}

	void inc() { ++_refCount; }

	bool indirect() const
	{
		return _refCount > 1 or isStream() or isDictionary();
	}

	void setID( int i ) { _id = i; }
	int getID() const { return _id; }

protected:
	PDFObject( type_t i_type ) : _type( i_type ) {}

	type_t _type;
	size_t _refCount{1};
	int _id{0};
};

class PDFBoolean : public PDFObject
{
public:
	PDFBoolean( bool i_value ) : PDFObject( type_boolean ), _value( i_value ) {}

	bool value() const { return _value; }

protected:
	bool _value;
};

class PDFNumber : public PDFObject
{
public:
	PDFNumber( int i_value ) : PDFObject( type_number ), _isInt( true )
	{
		u._intValue = i_value;
	}
	PDFNumber( float i_value ) : PDFObject( type_number ), _isInt( false )
	{
		u._floatValue = i_value;
	}

	bool isInt() const { return _isInt; }
	int intValue() const { return _isInt ? u._intValue : (int)u._floatValue; }
	float floatValue() const
	{
		return _isInt ? (float)u._intValue : u._floatValue;
	}

protected:
	bool _isInt;
	union
	{
		int _intValue;
		float _floatValue;
	} u;
};

class PDFString : public PDFObject
{
public:
	PDFString( const std::string &i_value )
		: PDFObject( type_string ), _value( i_value )
	{
	}

	const std::string &value() const { return _value; }

protected:
	std::string _value;
};

class PDFName : public PDFObject
{
public:
	PDFName( const std::string &i_value )
		: PDFObject( type_name ), _value( i_value )
	{
	}

	const std::string &value() const { return _value; }

protected:
	std::string _value;
};

class PDFArray : public PDFObject
{
public:
	PDFArray() : PDFObject( type_array ) {}

	size_t count() const { return _value.size(); }
	const PDFObject *value( size_t i_index ) const { return _value[i_index]; }

	void addValue( PDFObject *i_obj ) { _value.push_back( i_obj ); }

protected:
	std::vector<PDFObject *> _value;
};

class PDFDictionary : public PDFObject
{
public:
	PDFDictionary() : PDFObject( type_dict ) {}

	size_t count() const { return _order.size(); }
	const PDFObject *value( size_t i_index, std::string &o_key ) const
	{
		if ( i_index > count() )
			return 0;
		o_key = _order[i_index];
		return value( o_key );
	}
	const PDFObject *value( const std::string &i_key ) const
	{
		if ( _value.find( i_key ) != _value.end() )
			return _value.find( i_key )->second;
		else
			return 0;
	}

	void addValue( const std::string &i_key, PDFObject *i_obj )
	{
		_value.insert( std::make_pair( i_key, i_obj ) );
		_order.push_back( i_key );
	}

protected:
	std::map<std::string, PDFObject *> _value;
	std::vector<std::string> _order;
};

class PDFStream : public PDFObject
{
public:
	PDFStream( std::unique_ptr<PDFDictionary> &&i_dict, CFDataRef i_data,
			   CGPDFDataFormat i_format )
		: PDFObject( type_stream ),
		  _dict( std::move( i_dict ) ),
		  _data( i_data ),
		  _format( i_format )
	{
		CFRetain( _data );
	}

	virtual ~PDFStream() { CFRelease( _data ); }

	const PDFDictionary *dict() const { return _dict.get(); }
	CFDataRef data( CGPDFDataFormat &o_format ) const
	{
		o_format = _format;
		return _data;
	}

	mutable bool outputAsText{false};

protected:
	std::unique_ptr<PDFDictionary> _dict;
	CFDataRef _data;
	CGPDFDataFormat _format;
};

class PDFNull : public PDFObject
{
public:
	PDFNull() : PDFObject( type_null ){};
};

typedef struct PDFIdentifier_dummy *PDFIdentifier;

PDFIdentifier IDRef( CGPDFDictionaryRef obj )
{
	return (PDFIdentifier)obj;
}

PDFIdentifier IDRef( CGPDFStreamRef obj )
{
	return (PDFIdentifier)obj;
}

PDFIdentifier IDRef( CGPDFArrayRef obj )
{
	return (PDFIdentifier)obj;
}

PDFIdentifier IDRef( CGPDFObjectRef obj )
{
	switch ( CGPDFObjectGetType( obj ) )
	{
		case kCGPDFObjectTypeNull:
		case kCGPDFObjectTypeBoolean:
		case kCGPDFObjectTypeInteger:
		case kCGPDFObjectTypeReal:
		case kCGPDFObjectTypeName:
		case kCGPDFObjectTypeString:
			return (PDFIdentifier)obj;
		case kCGPDFObjectTypeArray:
		{
			CGPDFArrayRef v;
			bool res = CGPDFObjectGetValue( obj, kCGPDFObjectTypeArray, &v );
			assert( res );
			return (PDFIdentifier)v;
		}
		case kCGPDFObjectTypeDictionary:
		{
			CGPDFDictionaryRef v;
			bool res =
				CGPDFObjectGetValue( obj, kCGPDFObjectTypeDictionary, &v );
			assert( res );
			return (PDFIdentifier)v;
		}
		case kCGPDFObjectTypeStream:
		{
			CGPDFStreamRef v;
			bool res = CGPDFObjectGetValue( obj, kCGPDFObjectTypeStream, &v );
			assert( res );
			return (PDFIdentifier)v;
		}
		default:
			assert( false );
			break;
	}
	return 0;
}

struct Context
{
	std::vector<std::unique_ptr<PDFObject>> objectList;
	std::unordered_map<PDFIdentifier, PDFObject *> visited;
};

PDFObject *VisitDict( CGPDFDictionaryRef dict, Context &ctx );
void VisitObject( CGPDFObjectRef obj, Context &ctx );
void WriteObject( std::ostream &s, const PDFObject *obj );

void SavePDF( std::ostream &s, int majorVersion, int minorVersion,
			  const std::vector<std::unique_ptr<PDFObject>> &objectList,
			  PDFObject *i_root, PDFObject *i_info )
{
	// set the ID of all object that will be written as indirect object
	int i = 0;
	for ( auto &it : objectList )
	{
		if ( it->indirect() )
			it->setID( ++i );
	}

	// locate Contents
	for ( auto &it : objectList )
	{
		auto dict = it->asDictionary();
		if ( dict != nullptr )
		{
			auto obj = dict->value( "Type" );
			if ( obj != nullptr )
			{
				auto type = obj->asName();
				if ( type != nullptr )
				{
					if ( type->value() == "Page" )
					{
						obj = dict->value( "Contents" );
						if ( obj != nullptr )
						{
							auto contents = obj->asStream();
							if ( contents != nullptr )
								contents->outputAsText = true;
						}
					}
				}
			}
			continue;
		}
		auto stream = it->asStream();
		if ( stream != nullptr )
		{
			auto dict = stream->dict();
			auto obj = dict->value( "FunctionType" );
			if ( obj != nullptr )
			{
				auto num = obj->asNumber();
				if ( num != nullptr )
				{
					if ( num->intValue() == 4 )
						stream->outputAsText = true;
				}
			}
			continue;
		}
	}

	// header
	auto start = s.tellp();
	s << "%PDF-" << majorVersion << "." << minorVersion << std::endl;
	s << "%..." << std::endl;

	// write all objects and build the xref
	std::map<int, size_t> xref;
	for ( auto &current : objectList )
	{
		if ( current->indirect() )
		{
			xref.insert(
				std::make_pair( current->getID(), s.tellp() - start ) );
			s << current->getID() << " 0 obj\n";
			WriteObject( s, current.get() );
			s << "\nendobj\n";
		}
	}

	//	xref
	auto startXref = s.tellp();
	s << "xref\n";
	s << "0 " << xref.size() << std::endl;
	s << "0000000000 00000 n " << std::endl;
	for ( i = 1; i < xref.size() + 1; ++i )
		s << std::setfill( '0' ) << std::setw( 10 ) << xref[i] << " "
		  << "00000 n " << std::endl;

	// trailer
	s << "trailer" << std::endl;
	s << "<< /Size " << xref.size() << " /Root " << i_root->getID()
	  << " 0 R /Info " << i_info->getID() << " 0 R>>" << std::endl;
	s << "startxref" << std::endl;
	s << startXref - start << std::endl;
	s << "%%EOF" << std::endl;
}

std::string make_writable( const std::string &i_s )
{
	std::string n;
	n.reserve( i_s.size() );
	for ( auto it : i_s )
	{
		if ( it == ' ' )
			n.append( "#20" );
		else
			n.append( 1, it );
	}
	return n;
}

struct ConvData
{
	size_t size;
	std::unique_ptr<char[]> data;
};
ConvData convertData( CFDataRef data, bool doASCII )
{
	auto ptr = CFDataGetBytePtr( data );
	auto l = CFDataGetLength( data );

	ConvData cd;
	if ( not doASCII )
	{
		cd.size = l;
		cd.data.reset( new char[cd.size] );
		memcpy( cd.data.get(), ptr, cd.size );
		return cd;
	}
	std::ostringstream ostr;

	for ( CFIndex i = 0; i < l; )
	{
		int a = ( ( ptr[i] & 0xF0 ) >> 4 );
		int b = ptr[i] & 0x0F;

		char c = a >= 10 ? 'A' + a - 10 : '0' + a;
		ostr << c;
		c = b >= 10 ? 'A' + b - 10 : '0' + b;
		ostr << c;

		++i;
		if ( ( i % 40 ) == 0 )
			ostr << "\n";
	}
	ostr << "\n";

	auto s = ostr.str();
	cd.size = s.size();
	cd.data.reset( new char[cd.size] );
	memcpy( cd.data.get(), s.data(), cd.size );
	return cd;
}

void WriteObject( std::ostream &s, const PDFObject *obj )
{
	switch ( obj->type() )
	{
		case PDFObject::type_boolean:
			if ( obj->asBoolean()->value() )
				s << "true ";
			else
				s << "false ";
			break;
		case PDFObject::type_number:
			if ( obj->asNumber()->isInt() )
				s << obj->asNumber()->intValue();
			else
				s << obj->asNumber()->floatValue();
			break;
		case PDFObject::type_string:
			s << "(" << obj->asString()->value() << ")";
			break;
		case PDFObject::type_name:
			s << "/" << make_writable( obj->asName()->value() );
			break;
		case PDFObject::type_array:
			s << "[ ";
			for ( size_t i = 0; i < obj->asArray()->count(); ++i )
			{
				auto o = obj->asArray()->value( i );
				if ( o->indirect() )
					s << o->getID() << " 0 R ";
				else
				{
					WriteObject( s, o );
					s << " ";
				}
			}
			s << "]";
			break;
		case PDFObject::type_dict:
		{
			auto dict = obj->asDictionary();
			s << "<<\n";
			for ( size_t i = 0; i < dict->count(); ++i )
			{
				std::string name;
				auto o = dict->value( i, name );
				s << "/" << name << " ";
				if ( o->indirect() )
					s << o->getID() << " 0 R\n";
				else
				{
					WriteObject( s, o );
					s << "\n";
				}
			}
			s << ">>";
			break;
		}
		case PDFObject::type_stream:
		{
			auto dict = obj->asStream()->dict();
			s << "<<\n";
			CGPDFDataFormat format;
			auto data = obj->asStream()->data( format );

			bool doASCII = true;
			auto type = dict->value( "Type" );
			if ( type != nullptr and type->type() == PDFObject::type_name and
				 type->asName()->value() == "Metadata" )
				doASCII = false;
			else if ( obj->asStream()->outputAsText )
				doASCII = false;

			auto convData = convertData( data, doASCII );

			if ( doASCII )
			{
				if ( format == CGPDFDataFormatJPEGEncoded )
					s << "/Filter [/ASCIIHexDecode /DCTDecode]\n";
				else if ( format == CGPDFDataFormatJPEG2000 )
					s << "/Filter [/ASCIIHexDecode /JPXDecode]\n";
				else
					s << "/Filter /ASCIIHexDecode\n";
			}

			for ( size_t i = 0; i < dict->count(); ++i )
			{
				std::string name;
				auto o = dict->value( i, name );
				if ( name == "Filter" )
				{
					// skip
				}
				else if ( name == "Length" )
				{
					s << "/Length " << convData.size << "\n";
				}
				else
				{
					s << "/" << name << " ";
					if ( o->indirect() )
						s << o->getID() << " 0 R\n";
					else
					{
						WriteObject( s, o );
						s << "\n";
					}
				}
			}
			s << ">>\nstream\n";
			s.write( convData.data.get(), convData.size );
			s << "\nendstream";
			break;
		}
		case PDFObject::type_null:
			s << "null";
			break;
		default:
			assert( false );
			break;
	}
}

int main( int argc, char *const argv[] )
{
	if ( argc < 2 )
	{
		std::cout << "usage: " << argv[0] << "file [file...]" << std::endl;
		return -1;
	}

	for ( int i = 1; i < argc; ++i )
	{
		try
		{
			auto url = CFURLCreateFromFileSystemRepresentation(
				0, (const UInt8 *)argv[i], strlen( argv[i] ), false );
			if ( url == 0 )
				throw std::runtime_error( "error creating url" );
			auto doc = CGPDFDocumentCreateWithURL( url );
			CFRelease( url );
			if ( doc == 0 )
				throw std::runtime_error( "cannot open file" );

			int majorVersion, minorVersion;
			CGPDFDocumentGetVersion( doc, &majorVersion, &minorVersion );

			auto catalog = CGPDFDocumentGetCatalog( doc );
			auto info = CGPDFDocumentGetInfo( doc );

			// visit the whole hierarchy
			Context ctx;
			auto rootObj = VisitDict( catalog, ctx );
			auto infoObj = VisitDict( info, ctx );

			ctx.visited.clear();
			CGPDFDocumentRelease( doc );

			SavePDF( std::cout, majorVersion, minorVersion, ctx.objectList,
					 rootObj, infoObj );
		}
		catch ( std::exception &ex )
		{
			std::cerr << ex.what() << " -> " << argv[i] << std::endl;
		}
	}
	return 0;
}

void dictVisitor( const char *key, CGPDFObjectRef value, void *info )
{
	std::vector<std::string> *keys = (std::vector<std::string> *)info;
	keys->push_back( key );
}

PDFObject *VisitDict( CGPDFDictionaryRef dict, Context &ctx )
{
	auto visit = ctx.visited.find( IDRef( dict ) );
	if ( visit != ctx.visited.end() )
	{
		visit->second->inc();
		return visit->second;
	}

	std::vector<std::string> keys;
	CGPDFDictionaryApplyFunction( dict, dictVisitor, &keys );

	auto newDict = std::make_unique<PDFDictionary>();
	auto ptr = newDict.get();
	ctx.visited.insert( std::make_pair( IDRef( dict ), ptr ) );
	ctx.objectList.push_back( std::move( newDict ) );

	for ( auto &it : keys )
	{
		CGPDFObjectRef value;
		if ( CGPDFDictionaryGetObject( dict, it.c_str(), &value ) )
		{
			VisitObject( value, ctx );
			auto found = ctx.visited.find( IDRef( value ) );
			assert( found != ctx.visited.end() );
			ptr->addValue( it, found->second );
		}
	}
	return ptr;
}

void VisitStream( CGPDFStreamRef stream, Context &ctx )
{
	auto visit = ctx.visited.find( IDRef( stream ) );
	if ( visit != ctx.visited.end() )
	{
		visit->second->inc();
		return;
	}

	auto streamDict = CGPDFStreamGetDictionary( stream );

	std::vector<std::string> keys;
	CGPDFDictionaryApplyFunction( streamDict, dictVisitor, &keys );

	auto newDict = std::make_unique<PDFDictionary>();

	for ( auto &it : keys )
	{
		CGPDFObjectRef value;
		if ( CGPDFDictionaryGetObject( streamDict, it.c_str(), &value ) )
		{
			VisitObject( value, ctx );
			auto found = ctx.visited.find( IDRef( value ) );
			assert( found != ctx.visited.end() );
			newDict->addValue( it, found->second );
		}
	}

	CGPDFDataFormat format;
	auto data = CGPDFStreamCopyData( stream, &format );
	auto newStream =
		std::make_unique<PDFStream>( std::move( newDict ), data, format );
	CFRelease( data );
	ctx.visited.insert( std::make_pair( IDRef( stream ), newStream.get() ) );
	ctx.objectList.push_back( std::move( newStream ) );
}

void VisitArray( CGPDFArrayRef array, Context &ctx )
{
	auto visit = ctx.visited.find( IDRef( array ) );
	if ( visit != ctx.visited.end() )
	{
		visit->second->inc();
		return;
	}

	auto newArray = std::make_unique<PDFArray>();
	auto ptr = newArray.get();
	ctx.visited.insert( std::make_pair( IDRef( array ), ptr ) );
	ctx.objectList.push_back( std::move( newArray ) );

	for ( size_t i = 0; i < CGPDFArrayGetCount( array ); ++i )
	{
		CGPDFObjectRef value;
		if ( CGPDFArrayGetObject( array, i, &value ) )
		{
			VisitObject( value, ctx );
			auto found = ctx.visited.find( IDRef( value ) );
			assert( found != ctx.visited.end() );
			ptr->addValue( found->second );
		}
	}
}

void VisitNull( CGPDFObjectRef obj, Context &ctx )
{
	auto visit = ctx.visited.find( IDRef( obj ) );
	if ( visit != ctx.visited.end() )
	{
		visit->second->inc();
		return;
	}

	auto newNull = std::make_unique<PDFNull>();
	ctx.visited.insert( std::make_pair( IDRef( obj ), newNull.get() ) );
	ctx.objectList.push_back( std::move( newNull ) );
}

void VisitInteger( CGPDFObjectRef obj, Context &ctx )
{
	auto visit = ctx.visited.find( IDRef( obj ) );
	if ( visit != ctx.visited.end() )
	{
		visit->second->inc();
		return;
	}

	CGPDFInteger v;
	bool res = CGPDFObjectGetValue( obj, kCGPDFObjectTypeInteger, &v );
	assert( res );
	auto newNumber = std::make_unique<PDFNumber>( (int)v );
	ctx.visited.insert( std::make_pair( IDRef( obj ), newNumber.get() ) );
	ctx.objectList.push_back( std::move( newNumber ) );
}

void VisitFloat( CGPDFObjectRef obj, Context &ctx )
{
	auto visit = ctx.visited.find( IDRef( obj ) );
	if ( visit != ctx.visited.end() )
	{
		visit->second->inc();
		return;
	}

	CGPDFReal v;
	bool res = CGPDFObjectGetValue( obj, kCGPDFObjectTypeReal, &v );
	assert( res );
	auto newNumber = std::make_unique<PDFNumber>( (float)v );
	ctx.visited.insert( std::make_pair( IDRef( obj ), newNumber.get() ) );
	ctx.objectList.push_back( std::move( newNumber ) );
}

void VisitName( CGPDFObjectRef obj, Context &ctx )
{
	auto visit = ctx.visited.find( IDRef( obj ) );
	if ( visit != ctx.visited.end() )
	{
		visit->second->inc();
		return;
	}

	const char *v;
	bool res = CGPDFObjectGetValue( obj, kCGPDFObjectTypeName, &v );
	assert( res );
	auto newName = std::make_unique<PDFName>( v );
	ctx.visited.insert( std::make_pair( IDRef( obj ), newName.get() ) );
	ctx.objectList.push_back( std::move( newName ) );
}

void VisitString( CGPDFObjectRef obj, Context &ctx )
{
	auto visit = ctx.visited.find( IDRef( obj ) );
	if ( visit != ctx.visited.end() )
	{
		visit->second->inc();
		return;
	}

	CGPDFStringRef v;
	bool res = CGPDFObjectGetValue( obj, kCGPDFObjectTypeString, &v );
	assert( res );
	auto newString = std::make_unique<PDFString>( std::string(
		(const char *)CGPDFStringGetBytePtr( v ), CGPDFStringGetLength( v ) ) );
	ctx.visited.insert( std::make_pair( IDRef( obj ), newString.get() ) );
	ctx.objectList.push_back( std::move( newString ) );
}

void VisitBoolean( CGPDFObjectRef obj, Context &ctx )
{
	auto visit = ctx.visited.find( IDRef( obj ) );
	if ( visit != ctx.visited.end() )
	{
		visit->second->inc();
		return;
	}

	CGPDFBoolean b;
	bool res = CGPDFObjectGetValue( obj, kCGPDFObjectTypeBoolean, &b );
	assert( res );
	auto newBoolean = std::make_unique<PDFBoolean>( b );
	ctx.visited.insert( std::make_pair( IDRef( obj ), newBoolean.get() ) );
	ctx.objectList.push_back( std::move( newBoolean ) );
}

void VisitObject( CGPDFObjectRef obj, Context &ctx )
{
	auto visit = ctx.visited.find( IDRef( obj ) );
	if ( visit != ctx.visited.end() )
	{
		visit->second->inc();
		return;
	}

	switch ( CGPDFObjectGetType( obj ) )
	{
		case kCGPDFObjectTypeNull:
			VisitNull( obj, ctx );
			break;
		case kCGPDFObjectTypeBoolean:
			VisitBoolean( obj, ctx );
			break;
		case kCGPDFObjectTypeInteger:
			VisitInteger( obj, ctx );
			break;
		case kCGPDFObjectTypeReal:
			VisitFloat( obj, ctx );
			break;
		case kCGPDFObjectTypeName:
			VisitName( obj, ctx );
			break;
		case kCGPDFObjectTypeString:
			VisitString( obj, ctx );
			break;
		case kCGPDFObjectTypeArray:
		{
			CGPDFArrayRef v;
			bool res = CGPDFObjectGetValue( obj, kCGPDFObjectTypeArray, &v );
			assert( res );
			VisitArray( v, ctx );
			break;
		}
		case kCGPDFObjectTypeDictionary:
		{
			CGPDFDictionaryRef v;
			bool res =
				CGPDFObjectGetValue( obj, kCGPDFObjectTypeDictionary, &v );
			assert( res );
			VisitDict( v, ctx );
			break;
		}
		case kCGPDFObjectTypeStream:
		{
			CGPDFStreamRef v;
			bool res = CGPDFObjectGetValue( obj, kCGPDFObjectTypeStream, &v );
			assert( res );
			VisitStream( v, ctx );
			break;
		}
		default:
			assert( false );
			break;
	}
}
