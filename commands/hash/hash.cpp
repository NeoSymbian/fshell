// hash.cpp
// 
// Copyright (c) 2009 - 2010 Accenture. All rights reserved.
// This component and the accompanying materials are made available
// under the terms of the "Eclipse Public License v2.0"
// which accompanies this distribution, and is available
// at the URL "https://www.eclipse.org/legal/epl-2.0/".
// SPDX-License-Identifier: EPL-2.0
// Initial Contributors:
// Accenture - Initial contribution
// The NeoSymbian Authors (2026): binary-safe debug access.

#include <fshell/ioutils.h>
#include <fshell/ltkutils.h>
#include <fshell/common.mmh>
#include <hash.h>

const TInt KBlockSize = 512;

using namespace IoUtils;

class CCmdHash : public CCommandBase
	{
public:
	static CCommandBase* NewLC();
	~CCmdHash();
private:
	CCmdHash();
	CMessageDigest::THashId HashIdL();
private: // From CCommandBase.
	virtual const TDesC& Name() const;
	virtual void DoRunL();
	virtual void OptionsL(RCommandOptionList& aOptions);
private:
	TBool iVerbose;
	TBool iHex;
	TFileName2 iInputFileName;
	TFileName2 iOutputFileName;
	enum TAlgorithm
		{
		// Keep the order identical to hash.cif's algorithm enum.
		ESha1,
		EMd2,
		EMd4,
		EMd5,
		ESha224,
		ESha256,
		ESha384,
		ESha512
		} iAlgorithm;
	CMessageDigest* iMessageDigest;
	};


CCommandBase* CCmdHash::NewLC()
	{
	CCmdHash* self = new(ELeave) CCmdHash();
	CleanupStack::PushL(self);
	self->BaseConstructL();
	return self;
	}

CCmdHash::~CCmdHash()
	{
	delete iMessageDigest;
	}

CCmdHash::CCmdHash()
	{
	}

CMessageDigest::THashId CCmdHash::HashIdL()
	{
	switch (iAlgorithm)
		{
		case ESha1:
			return CMessageDigest::ESHA1;
		case EMd2:
			return CMessageDigest::EMD2;
#ifdef FSHELL_MD4_SUPPORT
		case EMd4:
			return CMessageDigest::EMD4;
#endif // FSHELL_MD4_SUPPORT
		case EMd5:
			return CMessageDigest::EMD5;
#ifdef FSHELL_SHA2_SUPPORT
		case ESha224:
			return CMessageDigest::ESHA224;
		case ESha256:
			return CMessageDigest::ESHA256;
		case ESha384:
			return CMessageDigest::ESHA384;
		case ESha512:
			return CMessageDigest::ESHA512;
#endif // FSHELL_SHA2_SUPPORT
		default:
			LeaveIfErr(KErrNotSupported, _L("Requested hashing algorithm is not available in this Symbian OS platform"));
			return CMessageDigest::ESHA1;
		}
	}

const TDesC& CCmdHash::Name() const
	{
	_LIT(KName, "hash");	
	return KName;
	}

void CCmdHash::DoRunL()
	{
	iMessageDigest = CMessageDigestFactory::NewDigestL(HashIdL());

	if (iOptions.IsPresent(&iInputFileName))
		{
		// RIoFile is a UTF-8 text endpoint. File digests must see every byte,
		// including malformed UTF-8, NUL and all values above 0x7f.
		RFile file;
		LeaveIfErr(file.Open(FsL(), iInputFileName, EFileRead | EFileShareReadersOnly), _L("Couldn't open '%S'"), &iInputFileName);
		CleanupClosePushL(file);
		TBuf8<KBlockSize> bytes;
		for (;;)
			{
			LeaveIfErr(file.Read(bytes), _L("Unable to read input file"));
			if (bytes.Length() == 0) break;
			iMessageDigest->Update(bytes);
			}
		CleanupStack::PopAndDestroy(&file);
		}
	else
		{
		// Retain the shell's existing text-pipeline input behavior.
		RIoReadHandle readHandle;
		CleanupClosePushL(readHandle);
		LeaveIfErr(readHandle.Create(IoSession()), _L("Couldn't create input handle"));
		LeaveIfErr(readHandle.Duplicate(Stdin()), _L("Couldn't duplicate STDIN"));
		LeaveIfErr(readHandle.SetReadMode(RIoReadHandle::EOneOrMore), _L("Couldn't set input mode"));
		TBuf<KBlockSize> buf;
		TInt err;
		while ((err = readHandle.Read(buf)) == KErrNone)
			{
			iMessageDigest->Update(buf.Collapse());
			}
		if (err != KErrEof) LeaveIfErr(err, _L("Unable to read input"));
		CleanupStack::PopAndDestroy(&readHandle);
		}

	TPtrC8 hash(iMessageDigest->Final());

	if (iOptions.IsPresent(&iOutputFileName))
		{
		RFile outputFile;
		LeaveIfErr(outputFile.Create(FsL(), iOutputFileName, EFileWrite), _L("Couldn't create '%S'"), &iOutputFileName);
		CleanupClosePushL(outputFile);
		LeaveIfErr(outputFile.Write(hash), _L("Unable to write hash"));
		LeaveIfErr(outputFile.Flush(), _L("Unable to flush hash"));
		CleanupStack::PopAndDestroy(&outputFile);
		}
	else if (iHex)
		{
		_LIT(KHex, "0123456789abcdef");
		TBuf<130> output;
		for (TInt i = 0; i < hash.Length(); ++i)
			{
			output.Append(KHex()[hash[i] >> 4]);
			output.Append(KHex()[hash[i] & 15]);
			}
		output.Append(_L("\r\n"));
		LeaveIfErr(Stdout().Write(output), _L("Unable to write hash"));
		}
	else
		{
		LtkUtils::HexDumpToOutput(hash, Stdout());
		}
	}

void CCmdHash::OptionsL(RCommandOptionList& aOptions)
	{
	aOptions.AppendBoolL(iVerbose, _L("verbose"));
	aOptions.AppendBoolL(iHex, _L("hex"));
	aOptions.AppendFileNameL(iInputFileName, _L("input"));
	aOptions.AppendFileNameL(iOutputFileName, _L("output"));
	aOptions.AppendEnumL((TInt&)iAlgorithm, _L("algorithm"));
	}


EXE_BOILER_PLATE(CCmdHash)

