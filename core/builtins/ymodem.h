// ymodem.h
// 
// Copyright (c) 2008 - 2010 Accenture. All rights reserved.
// This component and the accompanying materials are made available
// under the terms of the "Eclipse Public License v2.0"
// which accompanies this distribution, and is available
// at the URL "https://www.eclipse.org/legal/epl-2.0/".
// SPDX-License-Identifier: EPL-2.0
// Initial Contributors:
// Accenture - Initial contribution
// The NeoSymbian Authors (2026): verified UTF-8 file transfer.


#ifndef __YMODEM_H__
#define __YMODEM_H__

#include <fshell/ioutils.h>
#include "xmodem.h"

using namespace IoUtils;


class CCmdYmodem : public CCmdXmodem
	{
public:
	static CCommandBase* NewLC();
	~CCmdYmodem();
private:
	CCmdYmodem();
	void ReceiveFileL(const TFileName2& aName, TInt aSize);
private: // From CCommandBase.
	virtual const TDesC& Name() const;
	virtual void DoRunL();
	virtual void ArgumentsL(RCommandArgumentList& aArguments);
	virtual void OptionsL(RCommandOptionList& aOptions);
private:
	RArray<TFileName2> iFileNames;
	TFileName2 iOutputFileName;
	HBufC* iExpectedSha256;
	TBuf8<32> iExpectedDigest;
	};


#endif // __YMODEM_H__
