#include "SVM.h"
#include "TrainData.h"
#include "ParamGrid.h"
#include "Mat.h"
#include "GenericAsyncWorker.h"

Nan::Persistent<v8::FunctionTemplate> SVM::constructor;

NAN_MODULE_INIT(SVM::Init) {
  v8::Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(SVM::New);
  constructor.Reset(ctor);
  ctor->InstanceTemplate()->SetInternalFieldCount(1);
  ctor->SetClassName(Nan::New("SVM").ToLocalChecked());

	Nan::SetAccessor(ctor->InstanceTemplate(), FF_NEW_STRING("c"), c);
	Nan::SetAccessor(ctor->InstanceTemplate(), FF_NEW_STRING("coef0"), coef0);
	Nan::SetAccessor(ctor->InstanceTemplate(), FF_NEW_STRING("degree"), degree);
	Nan::SetAccessor(ctor->InstanceTemplate(), FF_NEW_STRING("gamma"), gamma);
	Nan::SetAccessor(ctor->InstanceTemplate(), FF_NEW_STRING("nu"), nu);
	Nan::SetAccessor(ctor->InstanceTemplate(), FF_NEW_STRING("p"), p);
	Nan::SetAccessor(ctor->InstanceTemplate(), FF_NEW_STRING("kernelType"), kernelType);
	Nan::SetAccessor(ctor->InstanceTemplate(), FF_NEW_STRING("classWeights"), classWeights);
	Nan::SetAccessor(ctor->InstanceTemplate(), FF_NEW_STRING("varCount"), varCount);
	Nan::SetAccessor(ctor->InstanceTemplate(), FF_NEW_STRING("isTrained"), isTrained);

	Nan::SetPrototypeMethod(ctor, "setParams", SetParams);
	Nan::SetPrototypeMethod(ctor, "train", Train);
	Nan::SetPrototypeMethod(ctor, "trainAuto", TrainAuto);
	Nan::SetPrototypeMethod(ctor, "predict", Predict);
	Nan::SetPrototypeMethod(ctor, "getSupportVectors", GetSupportVectors);
	Nan::SetPrototypeMethod(ctor, "getUncompressedSupportVectors", GetUncompressedSupportVectors);
	Nan::SetPrototypeMethod(ctor, "calcError", CalcError);
	Nan::SetPrototypeMethod(ctor, "save", Save);
	Nan::SetPrototypeMethod(ctor, "load", Load);

	Nan::SetPrototypeMethod(ctor, "trainAsync", TrainAsync);
	Nan::SetPrototypeMethod(ctor, "trainAutoAsync", TrainAutoAsync);

	target->Set(Nan::New("SVM").ToLocalChecked(), ctor->GetFunction());
};

NAN_METHOD(SVM::New) {
	FF_METHOD_CONTEXT("SVM::New");
	SVM* self = new SVM();
	self->svm = cv::ml::SVM::create();
	if (info.Length() > 0) {
		FF_ARG_OBJ(0, FF_OBJ args);

		Nan::TryCatch tryCatch;
		self->setParams(args);
		if (tryCatch.HasCaught()) {
			tryCatch.ReThrow();
		}
	}
	self->Wrap(info.Holder());
	FF_RETURN(info.Holder());
};

NAN_METHOD(SVM::SetParams) {
	FF_REQUIRE_ARGS_OBJ("SVM::SetParams");

	Nan::TryCatch tryCatch;
	FF_UNWRAP(info.This(), SVM)->setParams(args);
	if (tryCatch.HasCaught()) {
		tryCatch.ReThrow();
	}
	FF_RETURN(info.This());
};

NAN_METHOD(SVM::Train) {
	FF_METHOD_CONTEXT("SVM::Train");

	if (!FF_IS_INSTANCE(TrainData::constructor, info[0]) && !FF_IS_INSTANCE(Mat::constructor, info[0])) {
		FF_THROW("expected arg 0 to be an instance of TrainData or Mat");
	}

	SVM* self = FF_UNWRAP(info.This(), SVM);
	FF_VAL ret;
	if (FF_IS_INSTANCE(TrainData::constructor, info[0])) {
		FF_ARG_INSTANCE(0, cv::Ptr<cv::ml::TrainData> trainData, TrainData::constructor, FF_UNWRAP_TRAINDATA_AND_GET);
		FF_ARG_UINT_IFDEF(1, unsigned int flags, 0);
		ret = Nan::New(self->svm->train(trainData, flags));
	}
	else {
		FF_ARG_INSTANCE(0, cv::Mat samples, Mat::constructor, FF_UNWRAP_MAT_AND_GET);
		FF_ARG_UINT(1, unsigned int layout);
		FF_ARG_INSTANCE(2, cv::Mat responses, Mat::constructor, FF_UNWRAP_MAT_AND_GET);
		ret = Nan::New(self->svm->train(samples, (int)layout, responses));
	}
	FF_RETURN(ret);
}

NAN_METHOD(SVM::TrainAuto) {
	FF_METHOD_CONTEXT("SVM::TrainAuto");

	// required args
	FF_ARG_INSTANCE(0, cv::Ptr<cv::ml::TrainData> trainData, TrainData::constructor, FF_UNWRAP_TRAINDATA_AND_GET);

	// optional args
	bool hasOptArgsObj = FF_HAS_ARG(1) && info[1]->IsObject();
	FF_OBJ optArgs = hasOptArgsObj ? info[1]->ToObject() : FF_NEW_OBJ();
	FF_GET_UINT_IFDEF(optArgs, unsigned int kFold, "kFold", 10);
	FF_GET_INSTANCE_IFDEF(optArgs, cv::ml::ParamGrid cGrid, "cGrid", ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, ParamGrid, cv::ml::SVM::getDefaultGrid(cv::ml::SVM::C));
	FF_GET_INSTANCE_IFDEF(optArgs, cv::ml::ParamGrid gammaGrid, "gammaGrid", ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, ParamGrid, cv::ml::SVM::getDefaultGrid(cv::ml::SVM::GAMMA));
	FF_GET_INSTANCE_IFDEF(optArgs, cv::ml::ParamGrid pGrid, "pGrid", ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, ParamGrid, cv::ml::SVM::getDefaultGrid(cv::ml::SVM::P));
	FF_GET_INSTANCE_IFDEF(optArgs, cv::ml::ParamGrid nuGrid, "nuGrid", ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, ParamGrid, cv::ml::SVM::getDefaultGrid(cv::ml::SVM::NU));
	FF_GET_INSTANCE_IFDEF(optArgs, cv::ml::ParamGrid coeffGrid, "coeffGrid", ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, ParamGrid, cv::ml::SVM::getDefaultGrid(cv::ml::SVM::COEF));
	FF_GET_INSTANCE_IFDEF(optArgs, cv::ml::ParamGrid degreeGrid, "degreeGrid", ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, ParamGrid, cv::ml::SVM::getDefaultGrid(cv::ml::SVM::DEGREE));
	FF_GET_BOOL_IFDEF(optArgs, bool balanced, "balanced", false);
	if (!hasOptArgsObj) {
		FF_ARG_UINT_IFDEF(1, kFold, kFold);
		FF_ARG_INSTANCE_IFDEF(2, cGrid, ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, cGrid);
		FF_ARG_INSTANCE_IFDEF(3, gammaGrid, ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, gammaGrid);
		FF_ARG_INSTANCE_IFDEF(4, pGrid, ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, pGrid);
		FF_ARG_INSTANCE_IFDEF(5, nuGrid, ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, nuGrid);
		FF_ARG_INSTANCE_IFDEF(6, coeffGrid, ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, coeffGrid);
		FF_ARG_INSTANCE_IFDEF(7, degreeGrid, ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, degreeGrid);
		FF_ARG_BOOL_IFDEF(8, balanced, balanced);
	}

	bool ret = FF_UNWRAP(info.This(), SVM)->svm->trainAuto(trainData, (int)kFold, cGrid, gammaGrid, pGrid, nuGrid, coeffGrid, degreeGrid, balanced);
	FF_RETURN(Nan::New(ret));
}

NAN_METHOD(SVM::Predict) {
	FF_METHOD_CONTEXT("SVM::Predict");

	if (!info[0]->IsArray() && !FF_IS_INSTANCE(Mat::constructor, info[0])) {
		FF_THROW("expected arg 0 to be an ARRAY or an instance of Mat");
	}

	cv::Mat results;
	if (info[0]->IsArray()) {
		FF_ARG_UNPACK_FLOAT_ARRAY(0, samples);
		FF_ARG_UINT_IFDEF(1, unsigned int flags, 0);
		FF_UNWRAP(info.This(), SVM)->svm->predict(samples, results, (int)flags);
	}
	else {
		FF_ARG_INSTANCE(0, cv::Mat samples, Mat::constructor, FF_UNWRAP_MAT_AND_GET);
		FF_ARG_UINT_IFDEF(1, unsigned int flags, 0);
		FF_UNWRAP(info.This(), SVM)->svm->predict(samples, results, (int)flags);
	}

	FF_VAL jsResult;
	if (results.cols == 1 && results.rows == 1) {
		jsResult = Nan::New((double)results.at<float>(0, 0));
	}
	else {
		std::vector<float> resultsVec;
		results.col(0).copyTo(resultsVec);
		FF_PACK_ARRAY(resultArray, resultsVec);
		jsResult = resultArray;
	}
	FF_RETURN(jsResult);
}

NAN_METHOD(SVM::GetSupportVectors) {
	FF_OBJ jsSupportVectors = FF_NEW_INSTANCE(Mat::constructor);
	FF_UNWRAP_MAT_AND_GET(jsSupportVectors) = FF_UNWRAP(info.This(), SVM)->svm->getSupportVectors();
	FF_RETURN(jsSupportVectors);
}

NAN_METHOD(SVM::GetUncompressedSupportVectors) {
	FF_METHOD_CONTEXT("SVM::GetUncompressedSupportVectors");
#if CV_VERSION_MINOR < 2
	FF_THROW("getUncompressedSupportVectors not implemented for v3.0, v3.1");
#else
	FF_OBJ jsSupportVectors = FF_NEW_INSTANCE(Mat::constructor);
	FF_UNWRAP_MAT_AND_GET(jsSupportVectors) = FF_UNWRAP(info.This(), SVM)->svm->getUncompressedSupportVectors();
	FF_RETURN(jsSupportVectors);
#endif
}

NAN_METHOD(SVM::CalcError) {
	FF_METHOD_CONTEXT("SVM::CalcError");
	FF_ARG_INSTANCE(0, cv::Ptr<cv::ml::TrainData> trainData, TrainData::constructor, FF_UNWRAP_TRAINDATA_AND_GET);
	FF_ARG_BOOL(1, bool test);

	FF_OBJ jsResponses = FF_NEW_INSTANCE(Mat::constructor);
	float error = FF_UNWRAP(info.This(), SVM)->svm->calcError(trainData, test, FF_UNWRAP_MAT_AND_GET(jsResponses));

	FF_OBJ ret = FF_NEW_OBJ();
	Nan::Set(ret, FF_NEW_STRING("error"), Nan::New((double)error));
	Nan::Set(ret, FF_NEW_STRING("responses"), jsResponses);
	FF_RETURN(ret);
}

NAN_METHOD(SVM::Save) {
	FF_METHOD_CONTEXT("SVM::Save");
	FF_ARG_STRING(0, std::string path);
	FF_UNWRAP(info.This(), SVM)->svm->save(path);
}

NAN_METHOD(SVM::Load) {
	FF_METHOD_CONTEXT("SVM::Load");
	FF_ARG_STRING(0, std::string path);
#if CV_VERSION_MINOR < 2
	FF_UNWRAP(info.This(), SVM)->svm = cv::ml::SVM::load<cv::ml::SVM>(path);
#else
	FF_UNWRAP(info.This(), SVM)->svm = cv::ml::SVM::load(path);
#endif
}

void SVM::setParams(v8::Local<v8::Object> params) {
	FF_METHOD_CONTEXT("SVM::SetParams");

	FF_GET_NUMBER_IFDEF(params, double c, "c", this->svm->getC());
	FF_GET_NUMBER_IFDEF(params, double coef0, "coef0", this->svm->getCoef0());
	FF_GET_NUMBER_IFDEF(params, double degree, "degree", this->svm->getDegree());
	FF_GET_NUMBER_IFDEF(params, double gamma, "gamma", this->svm->getGamma());
	FF_GET_NUMBER_IFDEF(params, double nu, "nu", this->svm->getNu());
	FF_GET_NUMBER_IFDEF(params, double p, "p", this->svm->getP());
	FF_GET_NUMBER_IFDEF(params, unsigned int kernelType, "kernelType", this->svm->getKernelType());
	FF_GET_INSTANCE_IFDEF(params, cv::Mat classWeights, "classWeights", Mat::constructor, FF_UNWRAP_MAT_AND_GET, Mat, this->svm->getClassWeights());

	this->svm->setC(c);
	this->svm->setCoef0(coef0);
	this->svm->setDegree(degree);
	this->svm->setGamma(gamma);
	this->svm->setNu(nu);
	this->svm->setP(p);
	this->svm->setKernel(kernelType);
	this->svm->setClassWeights(classWeights);
}


NAN_METHOD(SVM::TrainAsync) {
	FF_METHOD_CONTEXT("SVM::TrainAsync");

	if (!FF_IS_INSTANCE(TrainData::constructor, info[0]) && !FF_IS_INSTANCE(Mat::constructor, info[0])) {
		FF_THROW("expected arg 0 to be an instance of TrainData or Mat");
	}

	cv::Ptr<cv::ml::SVM> svm = FF_UNWRAP(info.This(), SVM)->svm;
	FF_VAL ret;
	if (FF_IS_INSTANCE(TrainData::constructor, info[0])) {
		TrainFromTrainDataContext ctx;
		ctx.svm = svm;
		FF_ARG_INSTANCE(0, ctx.trainData, TrainData::constructor, FF_UNWRAP_TRAINDATA_AND_GET);
		FF_ARG_UINT_IFDEF_NOT_FUNC(1, ctx.flags, 0);

		FF_ARG_FUNC(info.Length() - 1, v8::Local<v8::Function> cbFunc);
		Nan::AsyncQueueWorker(new GenericAsyncWorker<TrainFromTrainDataContext>(
			new Nan::Callback(cbFunc),
			ctx
		));
	}
	else {
		TrainFromMatContext ctx;
		ctx.svm = svm;
		FF_ARG_INSTANCE(0, ctx.samples, Mat::constructor, FF_UNWRAP_MAT_AND_GET);
		FF_ARG_UINT(1, ctx.layout);
		FF_ARG_INSTANCE(2, ctx.responses, Mat::constructor, FF_UNWRAP_MAT_AND_GET);
		FF_ARG_FUNC(info.Length() - 1, v8::Local<v8::Function> cbFunc);
		Nan::AsyncQueueWorker(new GenericAsyncWorker<TrainFromMatContext>(
			new Nan::Callback(cbFunc),
			ctx
		));
	}
}

NAN_METHOD(SVM::TrainAutoAsync) {
	FF_METHOD_CONTEXT("SVM::TrainAutoAsync");

	TrainAutoContext ctx;
	ctx.svm = FF_UNWRAP(info.This(), SVM)->svm;
	// required args
	FF_ARG_INSTANCE(0, ctx.trainData, TrainData::constructor, FF_UNWRAP_TRAINDATA_AND_GET);

	// optional args
	bool hasOptArgsObj = FF_ARG_IS_OBJECT(1);
	FF_OBJ optArgs = hasOptArgsObj ? info[1]->ToObject() : FF_NEW_OBJ();
	FF_GET_UINT_IFDEF(optArgs, ctx.kFold, "kFold", 10);
	FF_GET_INSTANCE_IFDEF(optArgs, ctx.cGrid, "cGrid", ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, ParamGrid, cv::ml::SVM::getDefaultGrid(cv::ml::SVM::C));
	FF_GET_INSTANCE_IFDEF(optArgs, ctx.gammaGrid, "gammaGrid", ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, ParamGrid, cv::ml::SVM::getDefaultGrid(cv::ml::SVM::GAMMA));
	FF_GET_INSTANCE_IFDEF(optArgs, ctx.pGrid, "pGrid", ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, ParamGrid, cv::ml::SVM::getDefaultGrid(cv::ml::SVM::P));
	FF_GET_INSTANCE_IFDEF(optArgs, ctx.nuGrid, "nuGrid", ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, ParamGrid, cv::ml::SVM::getDefaultGrid(cv::ml::SVM::NU));
	FF_GET_INSTANCE_IFDEF(optArgs, ctx.coeffGrid, "coeffGrid", ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, ParamGrid, cv::ml::SVM::getDefaultGrid(cv::ml::SVM::COEF));
	FF_GET_INSTANCE_IFDEF(optArgs, ctx.degreeGrid, "degreeGrid", ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, ParamGrid, cv::ml::SVM::getDefaultGrid(cv::ml::SVM::DEGREE));
	FF_GET_BOOL_IFDEF(optArgs, ctx.balanced, "balanced", false);
	if (!hasOptArgsObj) {
		FF_ARG_UINT_IFDEF_NOT_FUNC(1, ctx.kFold, ctx.kFold);
		FF_ARG_INSTANCE_IFDEF(2, ctx.cGrid, ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, ctx.cGrid);
		FF_ARG_INSTANCE_IFDEF(3, ctx.gammaGrid, ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, ctx.gammaGrid);
		FF_ARG_INSTANCE_IFDEF(4, ctx.pGrid, ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, ctx.pGrid);
		FF_ARG_INSTANCE_IFDEF(5, ctx.nuGrid, ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, ctx.nuGrid);
		FF_ARG_INSTANCE_IFDEF(6, ctx.coeffGrid, ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, ctx.coeffGrid);
		FF_ARG_INSTANCE_IFDEF(7, ctx.degreeGrid, ParamGrid::constructor, FF_UNWRAP_PARAMGRID_AND_GET, ctx.degreeGrid);
		FF_ARG_BOOL_IFDEF_NOT_FUNC(8, ctx.balanced, ctx.balanced);
	}

	FF_ARG_FUNC(info.Length() - 1, v8::Local<v8::Function> cbFunc);
	Nan::AsyncQueueWorker(new GenericAsyncWorker<TrainAutoContext>(
		new Nan::Callback(cbFunc),
		ctx
	));
}
